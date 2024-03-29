
#include "cartosphere/dsht.hpp"

#include "cartosphere/utility.hpp"

#include "cartosphere/functions.hpp"

int
cs_index2(int B, int l, int m)
{
	// Format of harmonics: (l,m) with l the degree, m the order
	//
	//     For each row in each triangular part, m is constant,
	//     and as we move right in each row, l increments by 1.
	//     |<-------------- B items, row-major -------------->|
	//   - +--------------------------------------------------+
	//   ^ |_h(__0,_____0)_  h(1, 0)      ...      h(B-1,  0) | UPPER
	//   | | h(B-1,-(B-1)) |_h(1,_1)_     ...      h(B-1,  1) | TRIANGLE
	//   B |      ...          ...   |____...____      ...    | IS FOR
	//   v | h(  1,    -1)     ...     h(B-1,-1) | h(B-1,B-1) | m >= 0
	//   - +-------------------------------------+------------+
	//       STRICTLY LOWER TRIANGLE IS FOR m < 0

	// The upper triangular part is indexed like a regular square
	if (m >= 0)
	{
		return B * m + l;
	}
	// The strictly lower triangular part is indexed like a parallelogram
	// For the row m: there are (B-|m|) rows above it, and degree begins at |m|
	else
	{
		m = -m;
		return B * (B - m) + (l - m);
	}
}

int
cs_index2_assoc(int B, int l, int m)
{
	// Number of items before order m:
	//      First term: B
	//      Final term: B-(m-1)
	//      Number of terms: m
	// Number of items with order m but before degree l:
	return (2 * B + 1 - m) * m / 2 + (l - m);
}

void
cs_fds2ht(int B, const double* data, double* harmonics, const double* ws2)
{
	int N = 2 * B;

	// Clear output data and the entire scratchpad
	memset(harmonics, 0, B * B * sizeof(double));

	// Prepare for logging
	Eigen::IOFormat OctaveFmt(Eigen::StreamPrecision, 0, ", ", ";\n", "", "", "[", "]");

	// Retrieve relevant blocks from the workspace
	auto weights = ws2 + 4;
	auto trigs = ws2 + (4 + 3 * N);

	// Put all data into a matrix
	Eigen::Map<const MatrixRowMajor> M(data, N, N);
	if (FLAGS_minloglevel == 0)
	{
		LOG(INFO) << "M\n" << M.format(OctaveFmt);
	}

	// Make an 1xN array of weights
	Eigen::Map<const RowArray> W(weights, N);
	if (FLAGS_minloglevel == 0)
	{
		LOG(INFO) << "W\n" << W.format(OctaveFmt);
	}

	// For each degree and order, compute the fourier
#pragma omp parallel for if (B >= 128 && FLAGS_minloglevel > 0) num_threads(ThreadsMaximum)
	for (int task = 0; task < B * B; ++task)
	{
		// Split tasks into proper degrees and orders
		int l = (int)sqrt(task);
		int m = task - l * l - l;

		// W .* P
		auto rank = cs_ws2_rePlmCosRank(B, l, abs(m), ws2);

		Eigen::Map<const RowArray> P(rank, N);
		if (FLAGS_minloglevel == 0)
		{
			stringstream sst;
			sst << "P_{" << l << "," << m << "} = "
				<< P.format(OctaveFmt);
			LOG(INFO) << sst.str();
		}

		RowVector WP = (W * P).matrix();
		if (FLAGS_minloglevel == 0)
		{
			stringstream sst;
			sst << "                W.*P = "
				<< WP.format(OctaveFmt);
			LOG(INFO) << sst.str();
		}

		// (W .* P) * M
		RowVector WPM = WP * M;
		if (FLAGS_minloglevel == 0)
		{
			stringstream sst;
			sst << "            (W.*P)*M = "
				<< WPM.format(OctaveFmt);
			LOG(INFO) << sst.str();
		}

		// ((W .* P) * M) .* T
		if (m != 0)
		{
			int offset;
			// Compute the offset of the azimuthal trigs in ws2
			if (m > 0)
			{
				offset = N * (m - 1);
			}
			else
			{
				offset = N * (B - m - 2);
			}

			Eigen::Map<const RowArray> T(trigs + offset, N);
			if (FLAGS_minloglevel == 0)
			{
				stringstream sst;
				sst << "                   T = "
					<< T.format(OctaveFmt);
				LOG(INFO) << sst.str();
			}

			WPM.array() *= T.array();

			if (FLAGS_minloglevel == 0)
			{
				stringstream sst;
				sst << "       ((W.*P)*M).*T = "
					<< WPM.format(OctaveFmt);
				LOG(INFO) << sst.str();
			}
		}

		// sum((W .* P) * M) .* T)
		harmonics[cs_index2(B, l, m)] = WPM.sum();
	}

	if (FLAGS_minloglevel == 0)
	{
		stringstream sst;
		sst << "print harmonics per format\n";
		for (int m = 0; m < B; ++m)
		{
			for (int l = B - m; l < B; ++l)
			{
				sst << "  "
					<< "B_{" << l << "," << (m - B) << "} = "
					<< harmonics[cs_index2(B, l, (m - B))];
			}
			for (int l = m; l < B; ++l)
			{
				sst << "  "
					<< "B_{" << l << "," << m << "} = "
					<< harmonics[cs_index2(B, l, m)];
			}
			sst << "\n";
		}
		LOG(INFO) << sst.str();
	}
}

void
cs_ids2ht(int B, const double* harmonics, double* data, const double* ws2,
	fftw_real* pad, fftw_plan many_idct, fftw_plan many_idst)
{
	int N = 2 * B;

	// Prepare for logging
	Eigen::IOFormat OctaveFmt(Eigen::StreamPrecision, 0, ", ", ";\n", "", "", "[", "]");

	if (FLAGS_minloglevel == 0)
	{
		LOG(INFO) << "cs_ids2ht print harmonics per format";
		for (int m = 0; m < B; ++m)
		{
			stringstream sst;
			sst << "\t";
			for (int l = B - m; l < B; ++l)
			{
				sst << "h_{" << l << "," << (m - B) << "} = "
					<< harmonics[cs_index2(B, l, (m - B))]
					<< ", ";
			}
			for (int l = m; l < B; ++l)
			{
				sst << "h_{" << l << "," << m << "} = "
					<< harmonics[cs_index2(B, l, m)]
					<< ", ";
			}
			LOG(INFO) << sst.str();
		}
	}

	// Clear output data and the entire scratchpad
	memset(data, 0, N * N * sizeof(double));
	memset(pad, 0, N * N * 2 * sizeof(double));

	// Compute 1D fourier coefficients for the northern hemisphere first
	// Cosine and sine coefficients are interwoven in the same matrix!
	// The structure of the scratchpad:
	//      +-----+-----+-----+-----+
	//      | NxB | NxB | NxB | NxB |
	//      | amj | cos | bmj | sin |
	//      +-----+-----+-----+-----+
#pragma omp parallel for if (B >= 128) num_threads(ThreadsMaximum)
	for (int j = 0; j < N; ++j)
	{
		fftw_real* amj = pad + (2 * N * j);
		fftw_real* bmj = amj + N;
		// Retrieve renormalized P_{l,m} per x_{j}-file
		// This file is already in upper triangular form
		auto rePlmCos = cs_ws2_rePlmCosFile(B, j, ws2);
		// Compute the cosine coefficients
		for (int m = 0; m < B; ++m, ++amj)
		{
			// Compute element-wise product between...
			// 1: ROW m of UPPER TRIANGLE of HARMONICS
			// 2: ROW m of UPPER TRIANGLE rePlmCosFile for x_{j}
			auto row1 = harmonics + cs_index2(B, m, m);
			auto row2 = rePlmCos + cs_index2_assoc(B, m, m);
			for (int l = m; l < B; ++l)
			{
				*amj += row1[l - m] * row2[l - m];
			}
		}
		// Compute the sine coefficients
		for (int m = 1; m < B; ++m, ++bmj)
		{
			// Compute element-wise product between...
			// 1: ROW B-m of LOWER TRIANGLE of HARMONICS, shifted by m
			// 2: ROW   m of UPPER TRIANGLE rePlmCosFile for x_{j}
			auto row1 = harmonics + cs_index2(B, m, -m);
			auto row2 = rePlmCos + cs_index2_assoc(B, m, m);
			for (int l = m; l < B; ++l)
			{
				*bmj += row1[l - m] * row2[l - m];
			}
		}
		// Zero out the final sine coefficient
		*bmj++ = 0;
	}

	// Turn coefficients into data
	if (FLAGS_minloglevel == 0)
	{
		LOG(INFO) << "cs_ids2ht invokes cs_ids2ht_execute";
	}
	cs_ids2ht_execute(B, pad, data, many_idct, many_idst);
}

void
cs_ids2ht_dp(int B, const double* harmonics, double* partials, const double* ws2,
	fftw_real* pad, fftw_plan many_idct, fftw_plan many_idst)
{
	int N = 2 * B;

	// Prepare for logging
	Eigen::IOFormat OctaveFmt(Eigen::StreamPrecision, 0, ", ", ";\n", "", "", "[", "]");

	// Clear output data and the entire scratchpad
	memset(partials, 0, N * N * sizeof(double));
	memset(pad, 0, N * N * 2 * sizeof(double));

	// Compute 1D fourier coefficients for the northern hemisphere
	// The cs_ids2ht_execute will run two passes of idct & idst, and between
	// the two passes, the Coefficients will be modified to account for the
	// southern hemisphere
#pragma omp parallel for if (B >= 128) num_threads(ThreadsMaximum)
	for (int j = 0; j < N; ++j)
	{
		fftw_real* amj = pad + (2 * N * j);
		fftw_real* bmj = amj + N;
		// Retrieve d~P_{l,m} per x_{j}-file
		// This file is already in upper triangular form
		auto drePlmCos = cs_ws2_drePlmCosFile(B, j, ws2);
		// Compute the cosine coefficients
		for (int m = 0; m < B; ++m, ++amj)
		{
			// Compute element-wise product between...
			// 1: ROW m of UPPER TRIANGLE of HARMONICS
			// 2: ROW m of UPPER TRIANGLE drePlmCosFile for x_{j}
			auto row1 = harmonics + cs_index2(B, m, m);
			auto row2 = drePlmCos + cs_index2_assoc(B, m, m);
			for (int l = m; l < B; ++l)
			{
				*amj += row1[l - m] * row2[l - m];
			}
		}
		// Compute the sine coefficients
		for (int m = 1; m < B; ++m, ++bmj)
		{
			// Compute element-wise product between...
			// 1: ROW B-m of LOWER TRIANGLE of HARMONICS, shifted by m
			// 2: ROW   m of UPPER TRIANGLE drePlmCosFile for x_{j}
			auto row1 = harmonics + cs_index2(B, m, -m);
			auto row2 = drePlmCos + cs_index2_assoc(B, m, m);
			for (int l = m; l < B; ++l)
			{
				*bmj += row1[l - m] * row2[l - m];
			}
		}
		// Zero out the final sine coefficient
		*bmj++ = 0;
	}

	// Turn coefficients into partials
	if (FLAGS_minloglevel == 0)
	{
		LOG(INFO) << "cs_ids2ht_dp invokes cs_ids2ht_execute";
	}
	cs_ids2ht_execute(B, pad, partials, many_idct, many_idst);
}

void
cs_ids2ht_da(int B, const double* harmonics, double* partials, const double* ws2,
	fftw_real* pad, fftw_plan many_idct, fftw_plan many_idst)
{
	int N = 2 * B;

	// Prepare for logging
	Eigen::IOFormat OctaveFmt(Eigen::StreamPrecision, 0, ", ", ";\n", "", "", "[", "]");

	// Clear output data and the entire scratchpad
	memset(partials, 0, N * N * sizeof(double));
	memset(pad, 0, N * N * 2 * sizeof(double));

	// Compute 1D fourier coefficients for the northern hemisphere
	// The cs_ids2ht_execute will run two passes of idct & idst, and between
	// the two passes, the Coefficients will be modified to account for the
	// southern hemisphere
#pragma omp parallel for if (B >= 128) num_threads(ThreadsMaximum)
	for (int j = 0; j < N; ++j)
	{
		fftw_real* amj = pad + (2 * N * j);
		fftw_real* bmj = amj + N;
		// Retrieve P_{l,m} per x_{j}-file
		// This file is already in upper triangular form
		// Unlike the polar derivatives, the derivatives aren't needed here!
		auto rePlmCos = cs_ws2_rePlmCosFile(B, j, ws2);
		// Compute the cosine coefficients
		// Note that in this partial derivative, nothing contributes to a_{0}
		*amj++ = 0;
		for (int m = 1; m < B; ++m, ++amj)
		{
			// Compute element-wise product between...
			// 1: ROW B-m of LOWER TRIANGLE of HARMONICS, shifted by m
			// 2: ROW   m of UPPER TRIANGLE rePlmCosFile for x_{j}
			auto row1 = harmonics + cs_index2(B, m, -m);
			auto row2 = rePlmCos + cs_index2_assoc(B, m, m);
			for (int l = m; l < B; ++l)
			{
				// Extra m due to partial derivative w.r.t. phi
				*amj += m * row1[l - m] * row2[l - m];
			}
		}
		// Compute the sine coefficients
		for (int m = 1; m < B; ++m, ++bmj)
		{
			// Compute element-wise product between...
			// 1: ROW m of UPPER TRIANGLE of HARMONICS
			// 2: ROW m of UPPER TRIANGLE rePlmCosFile for x_{j}
			auto row1 = harmonics + cs_index2(B, m, m);
			auto row2 = rePlmCos + cs_index2_assoc(B, m, m);
			for (int l = m; l < B; ++l)
			{
				// Extra -m due to partial derivative w.r.t. phi
				*bmj += (-m) * row1[l - m] * row2[l - m];
			}
		}
		// Zero out the final sine coefficient
		*bmj++ = 0;
	}

	// Turn coefficients into partials
	if (FLAGS_minloglevel == 0)
	{
		LOG(INFO) << "cs_ids2ht_da invokes cs_ids2ht_execute";
	}
	cs_ids2ht_execute(B, pad, partials, many_idct, many_idst);
}

void
cs_ids2ht_plans(int B,
	fftw_real* pad, fftw_plan* ptr_many_idct, fftw_plan* ptr_many_idst)
{
	int N = 2 * B;

	// For each D{C,S}T-III...
	// Perform rank-1 (1-dimensional)
	int rank = 1;
	// ... of input length B
	int n[] = { B };
	// ... for N batches
	int howmany = { N };

	// The first input element is at
	fftw_real* in = pad;
	// The input of each batch is n-shaped (hence NULL)
	int* inembed = NULL;
	// The stride of each element within each input batch
	int istride = 1;
	// The stride of the first input element across all batches
	int idist = 2 * N;

	// The first output element is at
	fftw_real* out = in + B;
	// The output of each batch is n-shaped (hence NULL)
	int* onembed = NULL;
	// The stride of each element within each output batch
	int ostride = 1;
	// The stride of the first output element across all batches
	int odist = 2 * N;

	// Perform DCT-III for each batch
	fftw_r2r_kind kind[] = { FFTW_REDFT01 };
	// Default runtime flags
	auto flags = FFTW_ESTIMATE;

	// Create DCT-III plan using the advanced real-to-real interface
	*ptr_many_idct = fftw_plan_many_r2r(rank, n, howmany,
		in, inembed, istride, idist,
		out, onembed, ostride, odist,
		kind, flags);

	// Create DST-III plan using the advanced real-to-real interface
	in += N; out += N; kind[0] = FFTW_RODFT01;
	*ptr_many_idst = fftw_plan_many_r2r(rank, n, howmany,
		in, inembed, istride, idist,
		out, onembed, ostride, odist,
		kind, flags);
}

void
cs_ids2ht_execute(int B, fftw_real* pad, fftw_real* data,
	fftw_plan many_idct, fftw_plan many_idst)
{
	int N = 2 * B;

	// Prepare for logging
	Eigen::IOFormat OctaveFmt(Eigen::StreamPrecision, 0, ", ", ";\n", "", "", "[", "]");

	if (FLAGS_minloglevel == 0)
	{
		LOG(INFO) << "DCT-III coefficients\n";
		for (int j = 0; j < N; ++j)
		{
			stringstream sst;
			sst << "\t";
			for (int m = 0; m < B; ++m)
			{
				sst << "a_{" << m << "}^{(" << j << ")} = "
					<< pad[2 * N * j + m]
					<< ", ";
			}
			LOG(INFO) << sst.str();
		}
	}
	if (FLAGS_minloglevel == 0)
	{
		LOG(INFO) << "DST-III coefficients\n";
		for (int j = 0; j < N; ++j)
		{
			stringstream sst;
			sst << "\t";
			for (int m = 0; m < B; ++m)
			{
				sst << "b_{" << m << "}^{(" << j << ")} = "
					<< pad[2 * N * j + N + m]
					<< ", ";
			}
			LOG(INFO) << sst.str();
		}
	}

	// Account for normalization (FFTW to C++17)
	for (int j = 0; j < N; ++j)
	{
		// All DCT-III coefficients with m != 0 must be divided by 2
		auto* target = pad + (2 * N * j + 1);
		for (int k = 1; k < B; ++k)
		{
			*target++ *= 0.5;
		}
		// All DCT-III coefficients with m != B-1 must be divided by 2
		// Note that the final coefficient is zero, so it need not be modified
		target += B;
		for (int k = 0; k < B - 1; ++k)
		{
			*target++ *= 0.5;
		}
	}
	// Perform D{C,S}T-III
	fftw_execute(many_idct); fftw_execute(many_idst);
	// Copy results to the eastern hemisphere
	for (int j = 0; j < N; ++j)
	{
		// Aggregate data due to DCT-III
		auto* target = data + (N * j);
		auto* source = pad + (2 * N * j + B);
		for (int k = 0; k < B; ++k)
		{
			*target++ += *source++;
		}
		// Aggregate data due to DST-III
		target = data + (N * j);
		source += B;
		for (int k = 0; k < B; ++k)
		{
			*target++ += *source++;
		}
	}

	if (FLAGS_minloglevel == 0)
	{
		LOG(INFO) << "Cosine contributions\n";
		for (int j = 0; j < N; ++j)
		{
			LOG(INFO) << "\t"
				<< "a_{" << j << ",:} = "
				<< Eigen::Map<RowVector>(pad + (2 * N * j + B), B).format(OctaveFmt);
		}
	}
	if (FLAGS_minloglevel == 0)
	{
		LOG(INFO) << "Sine contributions\n";
		for (int j = 0; j < N; ++j)
		{
			LOG(INFO) << "\t"
				<< "b_{" << j << ",:} = "
				<< Eigen::Map<RowVector>(pad + (2 * N * j + N + B), B).format(OctaveFmt);
		}
	}

	// Tune coefficients for the western hemisphere
	for (int j = 0; j < N; ++j)
	{
		// For every two DCT-III columns, negate the second
		auto* target = pad + (2 * N * j + 1);
		for (int k = 0; k < B; k += 2, target += 2)
		{
			*target *= -1.0;
		}
		// For every two DST-III columns, negate the first
		target += B - 1;
		for (int k = B; k < N; k += 2, target += 2)
		{
			*target *= -1.0;
		}
	}
	// Perform D{C,S}T-III
	fftw_execute(many_idct); fftw_execute(many_idst);
	// Copy results to the western hemisphere
	for (int j = 0; j < N; ++j)
	{
		// Aggregate data due to DCT-III
		auto* target = data + (N * j + B);
		auto* source = pad + (2 * N * j + B);
		for (int k = B; k < N; ++k)
		{
			*target++ += *source++;
		}
		// Aggregate data due to DST-III
		target = data + (N * j + B);
		source += B;
		for (int k = B; k < N; ++k)
		{
			*target++ += *source++;
		}
	}

	if (FLAGS_minloglevel == 0)
	{
		LOG(INFO) << "Synthesized Data";
		for (int j = 0; j < N; ++j)
		{
			LOG(INFO) << "\t"
				<< "b_{" << j << ",:} = "
				<< Eigen::Map<RowVector>(data + (N * j), N).format(OctaveFmt);
		}
	}
}

double*
cs_make_ws2(int B)
{
	// Allocate workspace
	double* const ws2 = new double [cs_ws2_size(B)];
	cs_make_ws2(B, ws2);
	return ws2;
}

void
cs_make_ws2(int B, double* ws2)
{
	int N = 2 * B;

	// Prepare for logging
	Eigen::IOFormat OctaveFmt(Eigen::StreamPrecision, 0, ", ", ";\n", "", "", "[", "]");

	// Segmentize the workspace into multiple blocks
	double* const blocks[8] = {
		// Block 0: 4 elements
		// Element 0: bandlimit
		// Element 1-3: unused
		ws2,

		// Block 1: N elements
		// Stores the weights for each cell through w = P\(2pi/B delta)
		ws2 + 4,

		// Block 2: N elements
		// Stores the polar cosines: cos(theta_{j}) = x_{j}
		ws2 + (4 + N),

		// Block 3: N elements
		// Stores the polar sines: sin(theta_{j}) = y_{j} = sqrt(1-x^2)
		ws2 + (4 + 2 * N),

		// Block 4: (N-2)*N elements
		// Stores the azimuthal sines and cosines for each azimuth
		// First B-1 rows are cos(m phi_{k}^{*})
		// Final B-1 rows are sin(m phi_{k}^{*})
		ws2 + (4 + 3 * N), // co( phi_{k}) ... cos(m phi_{k}) for each k

		// Block 5: N*B*(B+1)/2 elements
		// Stores the C++17 renormalized ~P_{l,m} = q_{l}^{m}P_{l}^{m}(x_{j})
		// Dimensions: First j, then m, then l
		ws2 + (4 + 3 * N + (N - 2) * N),

		// Block 6: N*B*(B+1)/2 elements
		// Permutes the block above to perform the inverse transform
		// Dimensions: First l, then m, then j
		ws2 + (4 + 3 * N + (N - 2) * N + N * B * (B + 1) / 2),

		// Block 7: N*(B*(B+1)/2) elements
		// Stores the coefficients used to compute the gradient field
		// Dimensions: First l, then m, then j
		ws2 + (4 + 3 * N + (N - 2) * N + N * B * (B + 1)),
	};
	
	// [Block 0] Bandlimit
	blocks[0][0] = B;
	blocks[0][1] = 0xE;
	blocks[0][2] = 0xE;
	blocks[0][3] = 0xF;

	// [Block 1-2] Generate weights by solving, for 0 <= l < N = 2B,
	// \sum_{j=1}^{N}(P_{l}(cos(theta_{j})))w_{j}=(2pi/B)delta_{0,l}
	double* w = blocks[1];
	double* x = blocks[2];
	double* tempCosPls = blocks[3]; // Do not overwrite until moved!
	{
		// Compute the cosine of polar angles
		for (int j = 0; j < N; ++j)
		{
			x[j] = cos(M_PI / N * (j + 0.5));
		}

		// Compute weights form Legendre coefficients up to degree 2B-1
#pragma omp parallel for if (B >= 128) num_threads(ThreadsMaximum)
		for (int l = 0; l < N; ++l)
		{
			double* target = tempCosPls + (N * l);
			for (int j = 0; j < N; ++j)
			{
				target[j] = cs_legendre(l, x[j]);
			}
		}

		// Fill Eigen matrices A, b, solve Au=b, extract results
		auto A = Eigen::Map<MatrixRowMajor>(tempCosPls, N, N);
		ColVector b(N);
		memset(b.data(), 0, N * sizeof(double));
		b[0] = 2 * M_PI / B;
		
		// Temporarily enable multithreaded Eigen
#ifdef _OPENMP
		Eigen::setNbThreads(ThreadsMaximum);
#endif
		ColVector u = A.partialPivLu().solve(b); // PartialPivLU suffices
#ifdef _OPENMP
		Eigen::setNbThreads(1);
#endif
		memcpy(w, u.data(), N * sizeof(double));
	}
	
	if (FLAGS_minloglevel == 0)
	{
		LOG(INFO) << "cs_make_ws2 workspace block 1";
		LOG(INFO) << "  w = " << Eigen::Map<RowArray>(w, N);
	}

	if (FLAGS_minloglevel == 0)
	{
		LOG(INFO) << "cs_make_ws2 workspace block 2";
		LOG(INFO) << "  x = " << Eigen::Map<RowArray>(x, N);
	}

	// [Block 3, 5] Populate associated Legendre table recursively
	double* y = blocks[3];
	double* reCosPlms = blocks[5];
	{
		// Normalize tempCosPls=P_{l}^{0} to ~P_{l}^{0}
		// Move tempCosPls to correct location
		for (int l = 0; l <= B; ++l)
		{
			double* target = cs_ws2_rePlmCosRank(B, l, 0, ws2);
			double* source = tempCosPls + (N * l);
			
			// q_{l,0} = 1/sqrt(2) * sqrt((2l+1)/pi)
			double q_l_0 = M_SQRT1_2 / sqrt(M_PI) * sqrt(l + 0.5);
			for (int j = 0; j < N; ++j)
			{
				source[j] *= q_l_0;
			}
			memcpy(target, source, N * sizeof(double));
		}
		tempCosPls = nullptr;

		// Compute the abs(sin(@)) of polar angles
		for (int j = 0; j < N; ++j)
		{
			y[j] = sin(M_PI / N * (j + 0.5));
		}

		// Populate diagonal: P_{l}^{l} => P_{l+1}^{l^1}
		double* rP_l_ls = cs_ws2_rePlmCosRank(B, 0, 0, ws2);
		for (int l = 0; l < B - 1; ++l)
		{
			// pointer to ~P_{l+1,l+1}
			double* rP_lp1_lp1s = cs_ws2_rePlmCosRank(B, l + 1, l + 1, ws2);

			// r_{l,l} = q_{l+1,l+1} / q_{l,l}
			//         = sqrt(1+delta_l) * sqrt((2l+3)/(2l+2)) / (2l+1)
			// a_{l,l} = r_{l,l} * (2l+1)
			double a_l_l = sqrt((1 + (l == 0)) * (l + 1.5) / (l + 1));

			// ~P_{l+1,l+1}(x) = a_{l,l} y ~P_{l,l}(x)
			for (int j = 0; j < N; ++j)
			{
				rP_lp1_lp1s[j] = a_l_l * y[j] * rP_l_ls[j];
			}

			// Advance
			rP_l_ls = rP_lp1_lp1s;
		}

		// Populate off-diagonal: P_{l}^{l} => P_{l+1}^{l}
		for (int l = 1; l < B - 1; ++l)
		{
			// Pointer to ~P_{l,l}
			rP_l_ls = cs_ws2_rePlmCosRank(B, l, l, ws2);
			// Pointer to ~P_{l+1,l}
			double* rP_lp1_ls = cs_ws2_rePlmCosRank(B, l + 1, l, ws2);

			// r_{l,l} = q_{l+1,l}/q_{l,l}
			//         = sqrt(1+delta_l)*sqrt(2l+3)/(2l+1)
			// b_{l,l} = r_{l,l} (2l+1) because l>0
			double b_l_l = sqrt(2 * l + 3);

			// ~P_{l+1,l}(x) = b_{l,l} x ~P_{l,l}(x)
			for (int j = 0; j < N; ++j)
			{
				rP_lp1_ls[j] = b_l_l * x[j] * rP_l_ls[j];
			}
		}

		// Populate horizontally: P_{l-1}^{m} & P_{l}^{m} => P_{l+1}^{m}
#pragma omp parallel for if (B >= 128) num_threads(ThreadsMaximum)
		for (int m = 1; m < B - 1; ++m)
		{
			// Pointer to ~P_{l-1,m}
			double* rP_lm1_ms = cs_ws2_rePlmCosRank(B, m, m, ws2);
			// Pointer to ~P_{l,m}
			double* rP_l_ms = cs_ws2_rePlmCosRank(B, m + 1, m, ws2);
			for (int l = m + 1; l < B - 1; ++l)
			{
				// Pointer to ~P_{l+1,m}
				double* rP_lp1_ms = cs_ws2_rePlmCosRank(B, l + 1, m, ws2);

				// r_{l,m} = q_{l+1,m}/q_{l,m}
				//         = sqrt((2l+3)/(2l+1)) sqrt((l+1-m)/(l+1+m))
				// c_{l,m} = r_{l,m} (2l+1) / (l-m+1)
				//         = sqrt((2l+3)(2l+1)/((l+1-m)*(l+1+m))
				double c_l_m = sqrt(double(2 * l + 3) * (2 * l + 1) / ((l + 1 - m) * (l + 1 + m)));

				// r_{l-1,m} = q_{l+1,m}/q_{l-1,m}
				//           = sqrt((2l+3)/(2l-1)) sqrt((l+1-m)(l-m)/((l+1+m)/(l+m)))
				// c_{l-1,m} = r_{l-1,m} (l+m) / (l-m+1)
				//           = sqrt((2l+3)/(2l-1)*(l+m)*(l-m)/(l+1-m)/(l+1+m))
				double c_lm1_m = sqrt((l + 1.5) / (l - 0.5)
					* (l + m) / (l + 1 + m) * (l - m) / (l + 1 - m));

				// ~P_{l+1,m}(x) = c_{l,m} x ~P_{l,m}(x) - c_{l-1,m} ~P_{l-1,m}(x)
				for (int j = 0; j < N; ++j)
				{
					rP_lp1_ms[j] = c_l_m * x[j] * rP_l_ms[j] - c_lm1_m * rP_lm1_ms[j];
				}

				// Prepare for the next iteration
				rP_lm1_ms = rP_l_ms;
				rP_l_ms = rP_lp1_ms;
			}
		}
	}

	if (FLAGS_minloglevel == 0)
	{
		LOG(INFO) << "cs_make_ws2 workspace block 3";
		LOG(INFO) << "  y = " << Eigen::Map<RowArray>(y, N);
	}

	// [Block 4] Populate trig values for inverse transform
	double* trigs = blocks[4];
	{
		// Compute the cosine of azimuthal angles from m=1 to m=B-1
		for (int k = 0; k < N; ++k)
		{
			double phi = 2 * M_PI * ((k + 0.5) / N);
			double* ptr = trigs + k;
			for (int m = 1; m < B; ++m, ptr += N)
			{
				*ptr = cos(m * phi);
			}
		}
		// Compute the sine of azimuthal angles from m=1 to m=B-1
		for (int k = 0; k < N; ++k)
		{
			double phi = 2 * M_PI * ((k + 0.5) / N);
			double* ptr = trigs + ((B - 1) * N + k);
			for (int m = 1; m < B; ++m, ptr += N)
			{
				*ptr = sin(m * phi);
			}
		}
	}

	if (FLAGS_minloglevel == 0)
	{
		LOG(INFO) << "cs_make_ws2 workspace block 5\n";
		for (int l = 0; l < B; ++l)
		{
			for (int m = 0; m <= l; ++m)
			{
				double* Plms = cs_ws2_rePlmCosRank(B, l, m, ws2);
				LOG(INFO) << "\t"
					<< "~P_{" << l << "," << m << "} = "
					<< Eigen::Map<RowVector>(Plms, 1, N).format(OctaveFmt);
			}
		}
	}

	// [Block 6] Transposed table
	// Memory intensive (lots of strides), consider generating this from scratch
	const int fileSize = B * (B + 1) / 2;
	double** samples = (double **)malloc(fileSize * sizeof(double *));
	{
		// Generate N-striding pointers
		auto ptr_target = samples;
		for (int m = 0; m < B; ++m)
		{
			for (int l = m; l < B; ++l)
			{
				*ptr_target++ = cs_ws2_rePlmCosRank(B, l, m, ws2);
			}
		}
		// For each j, fetch from N-striding pointers
#pragma omp parallel for if (B >= 128) num_threads(ThreadsMaximum)
		for (int j = 0; j < N; ++j)
		{
			auto target = cs_ws2_rePlmCosFile(B, j, ws2);
			auto ptr_source = samples;
			for (int i = 0; i < fileSize; ++i)
			{
				// Copy value, 1-off the source pointer per file
				target[i] = *(ptr_source[i] + j);
			}
		}
	}
	free(samples);
	samples = nullptr;

	if (FLAGS_minloglevel == 0)
	{
		LOG(INFO) << "cs_make_ws2 workspace block 6\n";
		double* Plms = cs_ws2_rePlmCosFile(B, 0, ws2);
		for (int j = 0; j < N; ++j)
		{
			stringstream sst;
			sst << "\t"
				<< "For theta_{" << j << "}: ";
			for (int m = 0; m < B; ++m)
			{
				for (int l = m; l < B; ++l)
				{
					sst << "  ~P_{" << l << "," << m << "} = " << *Plms++;
				}
				sst << ", ";
			}
			LOG(INFO) << sst.str();
		}
	}

	// [Block 7] Blocks for derivatives
#pragma omp parallel for if (B >= 128) num_threads(ThreadsMaximum)
	for (int j = 0; j < N; ++j)
	{
		double* rP = cs_ws2_rePlmCosFile(B, j, ws2);
		double* drP = cs_ws2_drePlmCosFile(B, j, ws2);
		double* target = drP;
		// The first element in each file will not be used
		// But it will be cleared to facilitate inverse discrete transforms
		*target++ = 0;
		double x_j = x[j];
		double y_j = y[j];
		// D_theta (~P_{l,m}(cos(theta)))
		//    = (x ~P_{l,m}(x) - (l+m)q_{l,m}/q_{l-1,m} ~P_{l-1,m}(x))/y
		for (int m = 0; m < B; ++m)
		{
			for (int l = std::max(1, m); l < B; ++l, ++target)
			{
				if (l > m)
				{
					// q_{l,m}/q_{l-1,m} = sqrt((2l+1)/(2l-1)*(l-m)/(l+m))
					// d_{l-1,m} = (l+m) q_{l,m}/q_{l-1,m}
					//           = sqrt((2l+1)/(2l-1)*(l-m)*(l+m))
					double d_lm1_m = sqrt((l + 0.5) / (l - 0.5) * ((l - m) * (l + m)));
					*target = (x_j * l * rP[cs_index2_assoc(B, l, m)]
						- d_lm1_m * rP[cs_index2_assoc(B, l - 1, m)]) / y_j;
				}
				// Separate treatment for when l=m
				else
				{
					// q_{l,l}/q_{l,l-1} = sqrt(1+delta(l-1))/sqrt(2l)
					// e_{l,l-1} = q_{l,l}/q_{l,l-1} (2l) (-1)^{l-1}
					double e_l_lm1 = sqrt((1.0 + (l == 1)) * (2 * l));
					*target = e_l_lm1 * rP[cs_index2_assoc(B, l, l - 1)]
						- l * x_j / y_j * rP[cs_index2_assoc(B, l, l)];
				}
			}
		}
	}

	if (FLAGS_minloglevel == 0)
	{
		LOG(INFO) << "cs_make_ws2 workspace block 7\n";
		double* Plms = cs_ws2_drePlmCosFile(B, 0, ws2);
		for (int j = 0; j < N; ++j)
		{
			stringstream sst;
			sst << "\t"
				<< "For theta_{" << j << "}: ";
			for (int m = 0; m < B; ++m)
			{
				for (int l = m; l < B; ++l)
				{
					sst << "  d~P_{" << l << "," << m << "} = " << *Plms++;
				}
				sst << ", ";
			}
			LOG(INFO) << sst.str();
		}
	}
}

void
cs_free_ws2(double *ws2)
{
	free(ws2);
}

int
cs_ws2_size(int B)
{
	// See cs_make_ws2
	int N = 2 * B;
	return (4 + 3 * N + (N - 2) * N + N * B * (B + 1) / 2 * 3);
}

double*
cs_ws2_rePlmCosRank(int B, int l, int m, double* ws2)
{
	int N = 2 * B;

	// Structure of rePlmCosRank block (j=1,...,N in each block)
	//      +--l-m--+ -> indexing
	//      | (0,0) |
	//      +-------+--l-m--+
	//      | (1,0) | (1,1) |
	//      +-------+-------+-------+
	//      |  ...  |  ...  |  ...  |
	//      +-------+-------+-------+---l-m---+
	//      |(B-1,0)|(B-1,1)|  ...  |(B-1,B-1)|
	//      +-------+-------+-------+---------+

	double* rank = ws2 +
		(4 + 3 * N + (N - 2) * N + N * (l * (l + 1) / 2 + m));

	return rank;
}

const double*
cs_ws2_rePlmCosRank(int B, int l, int m, const double* ws2)
{
	auto rank = cs_ws2_rePlmCosRank(B, l, m, const_cast<double*>(ws2));
	return const_cast<const double*>(rank);
}

double*
cs_ws2_rePlmCosFile(int B, int j, double* ws2)
{
	int N = 2 * B;

	// Structure of rePlmCosFile block for each polar angle
	//      +-------+-------+-------+---------+ ---> indexing
	//      | (0,0) | (1,0) |  ...  | (B-1,0) |
	//      +-------+-------+-------+---------+
	//              | (1,1) |  ...  | (B-1,1) |
	//              +-------+-------+---------+
	//                      |  ...  |   ...   |
	//                      +-------+---------+
	//                              |(B-1,B-1)|
	//                              +---------+

	double* file = cs_ws2_rePlmCosRank(B, B, 0, ws2) + (B * (B + 1) / 2 * j);

	return file;
}

const double*
cs_ws2_rePlmCosFile(int B, int j, const double* ws2)
{
	auto file = cs_ws2_rePlmCosFile(B, j, const_cast<double*>(ws2));
	return const_cast<const double*>(file);
}

double*
cs_ws2_drePlmCosFile(int B, int j, double* ws2)
{
	int N = 2 * B;

	double* file = cs_ws2_rePlmCosFile(B, j, ws2) + N * B * (B + 1) / 2;

	return file;
}

const double*
cs_ws2_drePlmCosFile(int B, int j, const double* ws2)
{
	auto file = cs_ws2_drePlmCosFile(B, j, const_cast<double*>(ws2));
	return const_cast<const double*>(file);
}
