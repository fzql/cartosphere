
#include <argparse/argparse.hpp>
using argparse::ArgumentParser;
using argparse::default_arguments;
using argparse::nargs_pattern;

#include "cartosphere/shapefile.hpp"
using Cartosphere::ShapeFile;

#include "cartosphere/research.hpp"

#include "cartosphere/cartosphere.hpp"
using Cartosphere::SpectralGlobe;
using Cartosphere::FiniteElementGlobe;

#include "cartosphere/dsht.hpp"

#define GLOG_NO_ABBREVIATED_SEVERITIES
#include <glog/logging.h>

int
runDemo(const std::string&, const std::vector<std::string>&);

/* **************************** MAIN ENTRY POINT **************************** */
int
main(int argc, char* argv[])
{
	google::InitGoogleLogging(*argv);

	ArgumentParser program("cartosphere", "0.1.0-dev");

	// Demonstrative scenarios
	// cartosphere demo [args...]
	ArgumentParser demoCmd("demo");
	demoCmd.add_description("Run a demo");
	demoCmd.add_argument("scenario")
		.help("Specify which demo to run")
		.nargs(nargs_pattern::at_least_one)
		.default_value(std::vector{ std::string{"list"} })
#if defined(APPLE_LIKE)
		.metavar("SCENARIO...")
#endif
		;
	program.add_subparser(demoCmd);

	// Benchmark scenarios
	// cartosphere benchmark
	ArgumentParser benchmarkCmd("benchmark");
	benchmarkCmd.add_description("Run a benchmark");
	program.add_subparser(benchmarkCmd);

	// Visualization mode
	// cartosphere viz INPUT OUTPUT [-i INFMT] [-o OUTFMT]
	ArgumentParser vizCmd("viz");
	vizCmd.add_description("Visualize cartographic file");
	vizCmd.add_argument("input")
		.help("Path of input file/folder")
#if defined(APPLE_LIKE)
		.metavar("INPUT")
#endif
		;
	vizCmd.add_argument("output")
		.help("Path to output file/folder")
#if defined(APPLE_LIKE)
		.metavar("OUTPUT")
#endif
		;
	vizCmd.add_argument("-i", "--input-format")
		.help("Input format")
		.nargs(1)
		.default_value(std::string{ "shapefile" })
#if defined(APPLE_LIKE)
		.metavar("INFMT")
#endif
		;
	vizCmd.add_argument("-o", "--output-format")
		.help("Output format")
		.nargs(1)
		.default_value(std::string{ "matlab" })
#if defined(APPLE_LIKE)
		.metavar("OUTFMT")
#endif
		;
	program.add_subparser(vizCmd);

	// Add verbosity control
	program.add_argument("--verbose")
		.help("Generate more messages?")
		.default_value(false)
		.implicit_value(true);
	
	// Transform input file into a spherical cartogram
	// cartosphere transform INPUT OUTPUT [-i INFMT] [-o OUTFMT] [-m CSM]
	ArgumentParser transformCmd("transform");
	transformCmd.add_description("Generate a spherical cartogram.");
	transformCmd.add_argument("input")
		.help("Path of input file/folder")
#if defined(APPLE_LIKE)
		.metavar("INPUT")
#endif
		;
	transformCmd.add_argument("output")
		.help("Path to output file/folder")
#if defined(APPLE_LIKE)
		.metavar("OUTPUT")
#endif
		;
	transformCmd.add_argument("-i", "--input-format")
		.help("Input format")
		.nargs(1)
		.default_value(std::string{ "shapefile" })
#if defined(APPLE_LIKE)
		.metavar("INFMT")
#endif
		;
	transformCmd.add_argument("-m", "--mesh")
		.help("Set background mesh for FEM")
		.nargs(1)
#if defined(APPLE_LIKE)
		.metavar("CSMFILE")
#endif
		;
	transformCmd.add_argument("-b", "--bandlimit")
		.help("Specify bandlimit for spectral solver")
		.nargs(1)
		.default_value(std::int16_t{ 32 })
#if defined(APPLE_LIKE)
		.metavar("B")
#endif
		;
	transformCmd.add_epilog("Specifying -m(esh) will disable -b.");
	program.add_subparser(transformCmd);

	// Set Epilog
	program.add_epilog("See Z. Li and S. A. Aryana (2018).");

	// Parse command line
	try
	{
		program.parse_args(argc, argv);
	}
	catch (const std::runtime_error& err)
	{
		std::cerr << err.what() << "\n";
		std::cerr << program;
		std::exit(1);
	}

	// Priotize demo over all other operating modes
	if (program.is_subcommand_used("demo"))
	{
		auto demoArgs = demoCmd.get<std::vector<std::string>>("scenario");
		auto scenario = demoArgs.front();
		demoArgs.erase(demoArgs.begin());

		return runDemo(scenario, demoArgs);
	}

	// Check if verbose is used
	if (program["--verbose"] == true)
	{
		std::cout << "Verbosity enabled\n";
	}
	else
	{
		// Set glog message level to ignore INFO items
		FLAGS_minloglevel = 1;
	}

	// Benchmark the entire program
	if (program.is_subcommand_used("benchmark"))
	{
		std::cout << "[STARTING BENCHMARK]\n"
			<< "#1: Discrete Real S2-Fourier Transforms\n"
			<< "\n"
			<< "  hat(l,m)=1/(1+l+|m|), gone through cs_ids2ht then cs_fds2ht.\n"
			<< "  Max error is the largest absolute error among all harmonics.\n"
			<< "\n"
			<< "  | ## |  BW  | algorithm | makews (s) | ids2ht (s) | fds2ht (s) |  max error  |\n"
			<< "  | --:| ----:|:---------:| ----------:| ----------:| ----------:| -----------:|\n";
		// Save std::cout flags for later restoration
		std::ios oldCoutState(nullptr);
		oldCoutState.copyfmt(std::cout);

		// Bandlimits with default treatment: (0 <= i < 9)
		//  - 2, 4, 8, 16, 32, 64, 128, 256, 512
		// Bandlimits with special treatment: (9 <= i)
		//  - 1024, 2048
		for (size_t i = 0; i < 9; ++i)
		{
			int B = (int)pow(2, i + 1);
			if (FLAGS_minloglevel == 0)
			{
				LOG(INFO) << "Benchmark #1: B = " << B;
			}

			// Print table header
			std::cout << "  "
				<< "| " << std::setw(2) << (i + 1) << " "
				<< "| " << std::setw(4) << B;
			std::cout.copyfmt(oldCoutState);

			// Print the algorithm used for different bandlimits
			if (B <= 512)
			{
				std::cout << " | tablebase | ";
			}
			else
			{
				std::cout << " | recursive | ";
			}
			std::cout << std::flush;

			// Allocate data and randomize the harmonics (hats)
			double* hats = fftw_alloc_real(B * B);
			double* data = fftw_alloc_real(4 * B * B);

			// Experiment with a simple case and compare to matlab
			vector<double> coeffs(B * B);
			for (int l = 0; l < B; ++l)
			{
				for (int m = -l; m <= l; ++m)
				{
					coeffs[cs_index2(B, l, m)] = 1.0 / (l + abs(m) + 1);
				}
			}
			memcpy(hats, coeffs.data(), B * B * sizeof(double));

			// Make pad, plans, and workspace
			double* ws2;
			{
				auto begin = steady_clock::now();
				ws2 = cs_make_ws2(B);
				auto end = steady_clock::now();

				auto elapsed = (double)
					(duration_cast<milliseconds>(end - begin).count()) / 1000;
				std::cout << std::setw(10)
					<< std::fixed << std::setprecision(3) << elapsed << " | "
					<< std::flush;
				std::cout.copyfmt(oldCoutState);
			}
			// Perform inverse transform (synthesis)
			{
				auto begin = steady_clock::now();
				int N = 2 * B;
				fftw_real* pad = fftw_alloc_real(N * N * 2);
				fftw_plan many_idct, many_idst;
				cs_ids2ht_plans(B, pad, &many_idct, &many_idst);
				cs_ids2ht(B, hats, data, ws2, pad, many_idct, many_idst);
				fftw_destroy_plan(many_idct);
				fftw_destroy_plan(many_idst);
				fftw_free(pad);
				auto end = steady_clock::now();

				auto elapsed = (double)
					(duration_cast<milliseconds>(end - begin).count()) / 1000;
				std::cout << std::setw(10)
					<< std::fixed << std::setprecision(3) << elapsed << " | "
					<< std::flush;
				std::cout.copyfmt(oldCoutState);
			}
			// Perform forward transform (analysis)
			{
				auto begin = steady_clock::now();
				cs_fds2ht(B, data, hats, ws2);
				auto end = steady_clock::now();

				double maxError = 0;
				for (int l = 0; l < B; ++l)
				{
					for (int m = -l; m <= l; ++m)
					{
						double error = abs(
							coeffs[cs_index2(B, l, m)]
							- hats[cs_index2(B, l, m)]
						);
						if (std::isnan(error))
						{
							maxError = std::numeric_limits<double>::quiet_NaN();
							break;
						}
						else if (error > maxError)
						{
							maxError = error;
						}
					}
				}

				auto elapsed = (double)
					(duration_cast<milliseconds>(end - begin).count()) / 1000;
				std::cout << std::setw(10)
					<< std::fixed << std::setprecision(3) << elapsed << " | ";
				std::cout.copyfmt(oldCoutState);

				std::cout << std::setw(11) << maxError << " | \n";
				std::cout.copyfmt(oldCoutState);
			}
			// Free memory and reset std::cout
			cs_free_ws2(ws2);
			fftw_free(hats);
			fftw_free(data);
		}
		std::exit(0);
	}

	// Visualize a file
	if (program.is_subcommand_used("viz"))
	{
		auto inputPath = vizCmd.get<std::string>("input");
		auto inputFormat = vizCmd.get<std::string>("--input-format");
		std::cout << "Input path: " << inputPath
			<< " (format: " << inputFormat << ")\n";

		auto outputPath = vizCmd.get<std::string>("output");
		auto outputFormat = vizCmd.get<std::string>("--output-format");
		std::cout << "Output path: " << outputPath
			<< " (format: " << outputFormat << ")\n";

		// Visualize a shapefile
		if (inputFormat == "shapefile")
		{
			ShapeFile shapefile;
			std::string message;
			std::cout << "Initializing shapefile from " << inputPath << "...\n";
			if (!shapefile.open(inputPath, message))
			{
				std::cerr << "Error: " << message << "\n";
				std::exit(1);
			}
			std::cout << "Shapes loaded: " << shapefile.count() << "\n";

			if (outputFormat == "matlab")
			{
				std::cout << "Vizzing shapefile using matlab...\n";
				shapefile.to_matlab(outputPath);
				std::cout << "Vizzing complete!\n";
				std::exit(0);
			}

			std::cerr << "Unhandled output format: " << outputFormat << "\n";
			std::exit(1);
		}

		std::cerr << "Unhandled input format: " << inputFormat << "\n";
		std::exit(1);
	}

	if (program.is_subcommand_used("transform"))
	{
		auto inputPath = transformCmd.get<std::string>("input");
		auto inputFormat = transformCmd.get<std::string>("--input-format");
		std::cout << "Input path: " << inputPath
			<< " (format: " << inputFormat << ")\n";

		auto outputPath = transformCmd.get<std::string>("output");
		auto outputFormat = transformCmd.get<std::string>("--output-format");
		std::cout << "Output path: " << outputPath
			<< " (format: " << outputFormat << ")\n";
		
		std::cout << "Collecting points to be transformed...\n";
		vector<Cartosphere::Point> points;
		if (inputFormat == "shapefile")
		{
			ShapeFile shapefile;
			string error;
			if (!shapefile.open(inputPath, error))
			{
				std::cerr << "Error: " << error << '\n';
				std::exit(1);
			}
			points = shapefile.gather();
		}
		else
		{
			std::cerr << "Unknown input format.\n";
			std::exit(1);
		}

		// Check if a mesh is specified. If so, use the FEM approach.
		if (transformCmd.is_used("--mesh"))
		{
			auto meshPath = transformCmd.get<std::string>("--mesh");
			std::cout << "Mesh specified: " << meshPath << "\n";
			std::cout << "Invoking FEM implementation...\n";
			FiniteElementGlobe solver;

			solver.transform(points);
			std::exit(0);
		}
		else
		{
			// Use the S2kit-based approach.
			auto bandlimit = transformCmd.get<std::int16_t>("--bandlimit");
			std::cout << "Bandlimit specified: " << bandlimit << "\n";
			std::cout << "Invoking S2kit-based implementation...\n";
			SpectralGlobe solver;

			solver.transform(points);
			std::exit(0);
		}
	}

	// If command is not right, print program help
	std::cout << program;
	return 0;
}

int
runDemo(const std::string& name, const std::vector<std::string>& args)
{
	if (name == "default")
		return demo();
	
	if (name == "diffusion")
		return demo_diffusion();

	if (name == "seminar")
		return seminar();

	if (name == "quadrature")
		return demo_quadrature();

	if (name == "testobj")
		return test_obj();

	// if (name == "benchmark")
	// 	return benchmark();

	if (name == "precompute")
	{
		if (args.size() != 1)
		{
			std::cerr << "This demo needs 1 argument.\n";
			std::exit(1);
		}

		auto path = args[0];
		return precompute_weights(path);
	}

	if (name == "refine")
	{
		if (args.size() != 1)
		{
			std::cerr << "Needs 1 demo argument.\n";
			std::exit(1);
		}

		auto path = args[0];
		return refine(path);
	}

	// RESEARCH CODE
	if (name == "A")
		return research_a();

	if (name == "B")
		return research_b();

	if (name == "C")
	{
		if (args.size() != 2)
		{
			std::cerr << "Needs 2 demo arguments.\n";
			std::exit(1);
		}

		auto l = std::stoi(args[0]);
		auto m = std::stoi(args[1]);
		return research_c(l, m);
	}

	if (name == "CC")
	{
		for (int l = 1; l <= 3; ++l)
		{
			std::cout << "Y_" << l << "^" << 0 << "\n\n";
			research_c(l, 0, true);

			for (int m = 1; m <= l; ++m)
			{
				std::cout << "Y_" << l << "^" << m << "\n\n";
				research_c(l, m, true);

				std::cout << "Y_" << l << "^" << -m << "\n\n";
				research_c(l, -m, true);
			}
		}
		return 0;
	}

	if (name == "D")
		return research_d();

	if (name == "F")
		return research_f();

	if (name == "G")
	{
		if (args.size() != 1)
		{
			std::cerr << "Needs 1 demo argument.\n";
			std::exit(1);
		}

		auto folder = args[0];
		return research_g(folder);
	}

	if (name != "list")
	{
		std::cerr << "Unknown demo name\n";
	}

	std::cout << "Available demo SCENARIO:\n"
		<< "default            [---]\n"
		<< "diffusion          [---]\n"
		<< "seminar            [---]\n"
		<< "quadrature         [---]\n"
		<< "testobj            [---]\n"
		// << "benchmark          [---]\n"
		<< "precompute         [---]\n"
		<< "refine LEVEL       [---]\n"
		<< "A                  [Research A]\n"
		<< "B                  [Research B]\n"
		<< "C L M              [Research C]\n"
		<< "CC                 [Research CC]\n"
		<< "D                  [Research D]\n"
		<< "F                  [Research F]\n"
		<< "G SHAPEFILE        [Research G]\n\n"
		<< "Usage: cartosphere demo SCENARIO [ARGS...]\n";
	return 0;
}
