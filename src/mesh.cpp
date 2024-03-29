
#include "cartosphere/mesh.hpp"

#include "cartosphere/functions.hpp"

/* *************************** *
 * class Cartosphere::Preimage *
 * *************************** */
Cartosphere::Image
Cartosphere::Preimage::toImage() const
{
	Image image;

	// Find the magnitude of the projection onto the xy-axis
	double projection = sin(p);

	// Decompose into the three components
	image.x = projection * cos(a);
	image.y = projection * sin(a);
	image.z = cos(p);

	return image;
}

/* ************************ *
 * class Cartosphere::Image *
 * ************************ */
Cartosphere::Preimage
Cartosphere::Image::to_preimage() const
{
	Preimage preimage;

	preimage.p = acos(z);

	if (abs(x) + abs(y) > DoubleEpsilon)
	{
		preimage.a = atan2(y, x);
	}
	else
	{
		preimage.a = 0;
	}

	return preimage;
}

double
Cartosphere::distance(
	const Cartosphere::Image& a, const Cartosphere::Image& b)
{
	Cartosphere::Preimage pa = a.to_preimage();
	Cartosphere::Preimage pb = b.to_preimage();
	double value = cos(pa.p) * cos(pb.p) +
		sin(pa.p) * sin(pb.p) * cos(pa.a - pb.a);
	return acos(value);
}

double
Cartosphere::angle(const Cartosphere::Image& a,
	const Cartosphere::Image& b, const Cartosphere::Image& c)
{
	// The sides of a spherical triangle
	double BC = distance(b, c),		
		CA = distance(c, a),
		AB = distance(a, b);
	
	// Find the angle a-b-c, see Todhunter 1863
	if (AB < DoubleEpsilon || BC < DoubleEpsilon)
	{
		return M_PI_2;
	}
	else
	{
		return acos((cos(CA) - cos(AB) * cos(BC)) / (sin(AB) * sin(BC)));
	}
}

/* ************************ *
 * class Cartosphere::Point *
 * ************************ */
bool
Cartosphere::Point::isAntipodalTo(const Point& p) const
{
	return this != &p
		&& this->x() + p.x() == 0
		&& this->y() + p.y() == 0
		&& this->z() + p.z() == 0;
}

bool
Cartosphere::Point::isValid() const
{
	return p() == 0 && a() == 0
		&& x() == 0 && y() == 0 && z() == 0;
}

void
Cartosphere::Point::move(FL3 displacement)
{
	Cartosphere::Arc arc(*this, displacement);
	if (!arc.is_degenerate())
	{
		set(arc.local(displacement.norm2()));
	}
}

double
Cartosphere::Point::azimuth(const Point& other) const
{
	double t = sin(a() - other.a()) * sin(other.p());
	double b = cos(other.p()) * sin(p())
		- sin(other.p()) * cos(p()) * cos(a() - other.a());

	return atan2(t, b);
}

Cartosphere::Point
Cartosphere::midpoint(const Point& a, const Point& b)
{
	if (a.isAntipodalTo(b))
	{
		return Point();
	}

	FL3 middleofChord = (FL3)a.image() + (FL3)b.image();
	middleofChord /= 2;
	middleofChord.normalize();

	return Point(Image(middleofChord));
}

/* *************************** *
 * class Cartosphere::Triangle *
 * *************************** */
int
Cartosphere::Triangle::orientation() const
{
	// Form vector AB, AC
	FL3 AB = A.image() - B.image();
	FL3 BC = B.image() - C.image();
	FL3 OC = C.image();
	// Obtain mixed product
	double product = dot(cross(AB, BC), OC);
	if (product > 0)
	{
		return 1;
	}
	else if (product < 0)
	{
		return -1;
	}
	else
	{
		return 0;
	}
}

double
Cartosphere::Triangle::area() const
{
	// Form three arcs
	Arc BC(B, C), CA(C, A), AB(A, B);
	// Extract length of each arc
	double a = BC.length();
	double b = CA.length();
	double c = AB.length();
	// Use the spherical law of cosines to calculate three angles
	double A = acos((cos(a) - cos(b) * cos(c)) / (sin(b) * sin(c)));
	double B = acos((cos(b) - cos(c) * cos(a)) / (sin(c) * sin(a)));
	double C = acos((cos(c) - cos(a) * cos(b)) / (sin(a) * sin(b)));
	// Calculate the spherical excess
	double excess = A + B + C - M_PI;
	// Area is equal to the spherical excess
	return excess;
}

double
Cartosphere::Triangle::areaEuclidean() const
{
	// Form vector AB, AC
	FL3 AB = B.image() - A.image();
	FL3 AC = C.image() - A.image();
	// Calculate the area as half of the magnitude of AB x AC
	FL3 product = cross(AB, AC);
	return double(0.5) * product.norm2();
}

FL3
Cartosphere::Triangle::barycentric(const Point& p) const
{
	FL3 t;
	Arc a(B, C);
	t.x = a.distance(p) / a.distance(A);
	Arc b(C, A);
	t.y = b.distance(p) / b.distance(B);
	Arc c(A, B);
	t.z = c.distance(p) / c.distance(C);

	double sum = t.x + t.y + t.z;

	return t / sum;
}

Cartosphere::Point
Cartosphere::Triangle::centroid() const
{
	FL3 c = A.image().to_vector() + B.image().to_vector() + C.image().to_vector();
	return Cartosphere::Point(Cartosphere::Image(c.normalize()));
}

bool
Cartosphere::Triangle::contains(const Point& p) const
{
	return Arc(A, B).encloses(p)
		&& Arc(B, C).encloses(p)
		&& Arc(C, A).encloses(p);
}

double
Cartosphere::Triangle::diameter() const
{
	// Form vector AB, AC
	FL3 AB = B.image() - A.image();
	FL3 AC = C.image() - A.image();
	// The oriented unit normal to the planar triangle
	FL3 n = normalize(cross(AB, AC));
	// The distance from the plane ABC to O
	double d = dot(A.image(), n);
	return double(2) * acos(d);
}

Cartosphere::Cap
Cartosphere::Triangle::circumcircle() const
{
	return Cap(centroid(), diameter() / 2);
}

Cartosphere::Function
Cartosphere::Triangle::element(size_t index) const
{
	Function f;
	switch (index)
	{
	case 0:
	{
		// Construct f such that f(A)=1, f(B)=0, f(C)=0
		Arc arc(B, C);
		Point pole = arc.pole();
		double height = M_PI_2 - distance(pole, A);
		// The support of this function is supposed to be this triangle
		f = [height, pole](const Cartosphere::Point& x) -> double {
			return (M_PI_2 - distance(pole, x)) / height;
		};
	}
	break;
	case 1:
	{
		// Construct f such that f(A)=0, f(B)=1, f(C)=0
		Arc arc(C, A);
		Point pole = arc.pole();
		double height = M_PI_2 - distance(pole, B);
		// The support of this function is supposed to be this triangle
		f = [height, pole](const Cartosphere::Point& x) -> double {
			return (M_PI_2 - distance(pole, x)) / height;
		};
	}
	break;
	case 2:
	{
		// Construct f such that f(A)=0, f(B)=0, f(C)=1
		Arc arc(A, B);
		Point pole = arc.pole();
		double height = M_PI_2 - distance(pole, C);
		// The support of this function is supposed to be this triangle
		f = [height, pole](const Cartosphere::Point& x) -> double {
			return (M_PI_2 - distance(pole, x)) / height;
		};
	}
	break;
	default:
		f = [](const Cartosphere::Point& x) -> double {
			return 0;
		};
	}
	return f;
}

FL3
Cartosphere::Triangle::gradient(size_t index) const
{
	FL3 gradient;
	switch (index)
	{
	case 0:
	{
		Point Ap = Arc(B, C).pole();
		Arc arc(A, Ap);
		gradient = arc.tangent(0);
		double scalar = double(1) / (M_PI_2 - arc.length());
		gradient *= scalar;
	}
	break;
	case 1:
	{
		Point Bp = Arc(C, A).pole();
		Arc arc(B, Bp);
		gradient = arc.tangent(0);
		double scalar = double(1) / (M_PI_2 - arc.length());
		gradient *= scalar;
	}
	break;
	case 2:
	{
		Point Cp = Arc(A, B).pole();
		Arc arc(C, Cp);
		gradient = arc.tangent(0);
		double scalar = double(1) / (M_PI_2 - arc.length());
		gradient *= scalar;
	}
	break;
	default:
		gradient.x = 0;
		gradient.y = 0;
		gradient.z = 0;
	}
	return gradient;
}

FL3
Cartosphere::Triangle::gradient(size_t index, const Point &p) const
{
	FL3 gradient;
	switch (index)
	{
	case 0:
	{
		Point Ap = Arc(B, C).pole();
		Arc arc(p, Ap);
		gradient = arc.tangent(0);
		double scalar = double(1) / (M_PI_2 - Arc(A, Ap).length());
		gradient *= scalar;
	}
	break;
	case 1:
	{
		Point Bp = Arc(C, A).pole();
		Arc arc(p, Bp);
		gradient = arc.tangent(0);
		double scalar = double(1) / (M_PI_2 - Arc(B, Bp).length());
		gradient *= scalar;
	}
	break;
	case 2:
	{
		Point Cp = Arc(A, B).pole();
		Arc arc(p, Cp);
		gradient = arc.tangent(0);
		double scalar = double(1) / (M_PI_2 - Arc(C, Cp).length());
		gradient *= scalar;
	}
	break;
	default:
		gradient.x = 0;
		gradient.y = 0;
		gradient.z = 0;
	}
	return gradient;
}

double
Cartosphere::Triangle::integrate(const Function& f, Integrator intr) const
{
	double integral = 0;
	switch (intr)
	{
	case Integrator::Centroid:
	{
		// Taking the function value at the centroid
		integral = f(centroid()) * area();
	} break;
	case Integrator::ThreeVertices:
	{
		// Taking the averages of the three vertices
		integral = (f(A) + f(B) + f(C)) / 3 * area();
	} break;
	case Integrator::Simpsons:
	{
		// Analogy to Simpson's rule
		integral = (f(A) + f(B) + f(C) + 3 * f(centroid())) / 6 * area();
	} break;
	case Integrator::Refinement1:
	case Integrator::Refinement2:
	case Integrator::Refinement3:
	case Integrator::Refinement4:
	case Integrator::Refinement5:
	case Integrator::Refinement6:
	case Integrator::Refinement7:
	case Integrator::Refinement8:
	case Integrator::Refinement9:
	case Integrator::Refinement10:
	{
		// Create a mesh
		TriangularMesh m;
		m.load(*this);
		// Refine five levels
		unsigned levels = 0;
		switch (intr)
		{
		case Integrator::Refinement1:
			levels = 1; break;
		case Integrator::Refinement2:
			levels = 2; break;
		case Integrator::Refinement3:
			levels = 3; break;
		case Integrator::Refinement4:
			levels = 4; break;
		case Integrator::Refinement5:
			levels = 5; break;
		case Integrator::Refinement6:
			levels = 6; break;
		case Integrator::Refinement7:
			levels = 7; break;
		case Integrator::Refinement8:
			levels = 8; break;
		case Integrator::Refinement9:
			levels = 9; break;
		case Integrator::Refinement10:
			levels = 10; break;
	case Integrator::Centroid:
	case Integrator::ThreeVertices:
	case Integrator::Simpsons:
	default:
		break;
		}
		for (unsigned k = 0; k < levels; ++k)
		{
			m.refine();
		}
		// Integrate through each refined simplex
		integral = m.integrate(f,
			TriangularMesh::Quadrature::AreaWeighted,
			Integrator::Centroid);
	} break;
	}
	return integral;
}

double
Cartosphere::Polygon::area() const
{
	double sum = 0;

	// Accumulate the sum of angles
	const size_t n = _V.size();
	for (size_t i = 0; i < _V.size(); ++i)
	{
		sum += angle(_V[i], _V[(i + 1) % n], _V[(i + 2) % n]);
	}

	// Return the spherical excess
	return sum - (n - 2) * M_PI;
}

void
Cartosphere::TriangularMesh::Tree::_insert(Node::Pointer n, size_t level)
{
	Node::Pointer subtree = _choose(n, level);

	if (subtree->degree() <= M)
	{
		subtree->store(subtree);
	}

	Node::Pointer iter = subtree;
	while (iter->degree() > M)
	{
		bool split = _overflow(iter, iter->height());

		if (!split) break;

		if (iter->isRoot())
		{
			// TODO
		}
	}

	// Adjust all circumcircles in the insertion path
	for (iter = subtree; !iter->isRoot(); iter = iter->parent())
	{
		// TODO
	}
}

Cartosphere::TriangularMesh::Tree::Node::Pointer
Cartosphere::TriangularMesh::Tree::_choose(Node::Pointer n, size_t level)
{
	// Based on the R-tree, not the R*-tree
	// This is because the overlap enlargement might be difficult to compute
	Node::Pointer iter = make_shared<Node>(_root);

	while (!iter->isLeaf())
	{
		typedef std::tuple<Node::Pointer, double, double> PointerWithAreas;
		
		vector<PointerWithAreas> areas;
		std::transform(iter->begin(), iter->end(), std::back_inserter(areas),
			[n](const Node::Pointer& a) {
				double before = a->cap().radius();
				//double after = a->cap().combine(n->cap()).radius();
				double after = 0;
				return std::make_tuple(a, after - before, before);
			}
		);

		std::sort(areas.begin(), areas.end(),
			[](const PointerWithAreas& a, const PointerWithAreas& b) {
				return std::get<1>(a) < std::get<1>(b)
					|| std::get<2>(a) < std::get<2>(b);
			}
		);

		iter = std::get<0>(areas.front());
	}

	return iter;
}

bool
Cartosphere::TriangularMesh::Tree::_overflow(Node::Pointer n, size_t level)
{
	if (level != height() && !_overflown[level])
	{
		_reinsert(n);
		return false;
	}
	else
	{
		_split(n);
		return true;
	}
}

void
Cartosphere::TriangularMesh::Tree::_reinsert(Node::Pointer n)
{
	auto& cap = n->cap();

	typedef std::pair<Node::Pointer, double> PointerWithDistance;
	vector<PointerWithDistance> dist;

	std::transform(n->begin(), n->end(), std::back_inserter(dist),
		[&cap](Node::Pointer n) {
			double d = distance(cap.apex(), n->cap().apex());
			return std::make_pair(n, d);
		}
	);

	std::sort(dist.begin(), dist.end(),
		[](const PointerWithDistance& a, const PointerWithDistance& b) {
			return a.second > b.second;
		}
	);

	Node::Pointers toRemove;
	std::transform(dist.begin(), dist.begin() + p, std::back_inserter(toRemove),
		[](const PointerWithDistance& a) {
			return a.first;
		}
	);

	Node::Pointers toRemain;
	std::transform(dist.begin() + p, dist.end(), std::back_inserter(toRemain),
		[](const PointerWithDistance& a) {
			return a.first;
		}
	);

	n->update(toRemain);

	for (int i = 0; i < toRemain.size(); ++i)
	{
		_insert(toRemain[i]);
	}
}

void
Cartosphere::TriangularMesh::Tree::_split(Node::Pointer n)
{

}

double
Cartosphere::TriangularMesh::integrate(
	const vector<double>& values,
	Quadrature rule, Triangle::Integrator intr) const
{
	double integral = 0;

	// The discrete values are for each dual polygon
	if (rule == Quadrature::DualAreaWeighted)
	{
		if (intr == Triangle::Integrator::Centroid)
		{
			// For each vertex v in the triangular mesh, we construct its dual
			// spherical polygon by storing its
			//  1. dual vertices
			//  2. azimuth of its dual vertex relative to v.
			vector<std::pair<Point, double>> dual;
			vector<Point> vertices;

			// For each vertex, construct its dual spherical polygon
			for (size_t i = 0; i < _V.size(); ++i)
			{
				// Convert the list of neighboring faces to the list of their centroids
				const Point& v = _V[i];
				const auto& fs = _VF[i];
				std::transform(fs.begin(), fs.end(), std::back_inserter(dual),
					[this, v](size_t k) {
						Point centroid = _vt[k].centroid();
						double azimuth = v.azimuth(centroid);
						return std::make_pair(centroid, azimuth);
					}
				);

				// Sort the dual vertices by azimuth
				struct AzimuthSorter
				{
					inline bool operator()(
						const std::pair<Point, double>& a,
						const std::pair<Point, double>& b) const
					{
						return a.second < b.second;
					}
				};
				std::sort(dual.begin(), dual.end(), AzimuthSorter());

				// Extract points to vector
				for (auto it = std::make_move_iterator(dual.begin()),
					end = std::make_move_iterator(dual.end()); it != end; ++it)
				{
					vertices.insert(vertices.end(), it->first);
				}

				// Spherical excess is the area
				// Only works if the azimuths all lie within the principal interval
				Polygon poly(vertices);
				double area = poly.area();

				// Accumulate the integral
				integral += pow(abs(values[i]), 2) * area;

				// Cleanup
				dual.clear();
				vertices.clear();
			}
		}
		else
		{
			for (size_t i = 0; i < _vt.size(); ++i)
			{
				const Triangle& t = _vt[i];

				// Obtain the restriction of basis functions
				auto e_a = t.element(0);
				auto e_b = t.element(1);
				auto e_c = t.element(2);

				// Obtain the values
				size_t index_a;
				{
					const auto& edge = std::get<0>(_F[i]);
					index_a = edge.second ? _E[edge.first].first : _E[edge.first].second;
				}
				size_t index_b;
				{
					const auto& edge = std::get<1>(_F[i]);
					index_b = edge.second ? _E[edge.first].first : _E[edge.first].second;
				}
				size_t index_c;
				{
					const auto& edge = std::get<2>(_F[i]);
					index_c = edge.second ? _E[edge.first].first : _E[edge.first].second;
				}
				auto f_a = values[index_a];
				auto f_b = values[index_b];
				auto f_c = values[index_c];

				auto f = [&e_a, &e_b, &e_c, f_a, f_b, f_c](const Point& p) {
					return pow(f_a * e_a(p) + f_b * e_b(p) + f_c * e_c(p), 2);
				};
				integral += t.integrate(f, intr);
			}
		}
	}

	return integral;
}

double
Cartosphere::TriangularMesh::lebesgue(
	const vector<double>& weights, const Function& func,
	Triangle::Integrator intr) const
{
	double integral = 0;

	// Integrate error in each triangle
	for (size_t i = 0; i < _vt.size(); ++i)
	{
		const Triangle& t = _vt[i];

		// Obtain the restriction of basis functions
		auto e_a = t.element(0);
		auto e_b = t.element(1);
		auto e_c = t.element(2);

		// Obtain the values
		size_t index_a;
		{
			const auto& edge = std::get<0>(_F[i]);
			index_a = edge.second ? _E[edge.first].first : _E[edge.first].second;
		}
		size_t index_b;
		{
			const auto& edge = std::get<1>(_F[i]);
			index_b = edge.second ? _E[edge.first].first : _E[edge.first].second;
		}
		size_t index_c;
		{
			const auto& edge = std::get<2>(_F[i]);
			index_c = edge.second ? _E[edge.first].first : _E[edge.first].second;
		}
		auto f_a = weights[index_a];
		auto f_b = weights[index_b];
		auto f_c = weights[index_c];

		auto f = [&e_a, &e_b, &e_c, f_a, f_b, f_c, func](const Point& p) {
			return pow(abs(f_a * e_a(p) + f_b * e_b(p) + f_c * e_c(p) - func(p)), 2);
		};
		integral += t.integrate(f, intr);
	}

	return pow(integral, 1 / double(2));
}

void
Cartosphere::TriangularMesh::clear()
{
	// Reset state variables
	_V.clear();
	_E.clear();
	_F.clear();
	_vt.clear();
	_vInfo.clear();

	// Reset state flags
	_bLoadSuccess = false;
	_bParseSuccess = false;
}

double
Cartosphere::TriangularMesh::area() const
{
	double area = 0;
	for (auto& simplex : _vt)
	{
		area += simplex.area();
	}
	return area;
}

double
Cartosphere::TriangularMesh::areaEuclidean() const
{
	double area = 0;
	for (auto& simplex : _vt)
	{
		area += simplex.areaEuclidean();
	}
	return area;
}

bool
Cartosphere::TriangularMesh::load(const Triangle& t)
{
	if (_bLoadSuccess || _bParseSuccess) clear();

	// Push V, E, and F information
	// V = 3
	_V.push_back(t.A);
	_V.push_back(t.B);
	_V.push_back(t.C);
	// E = 3
	_E.emplace_back(0, 1);
	_E.emplace_back(1, 2);
	_E.emplace_back(2, 0);
	// F = 1
	_F.emplace_back(
		DirectedEdge(0, true), DirectedEdge(1, true), DirectedEdge(2, true)
	);

	// Fill in the rest of the details
	_populate();
	return isReady();
}

bool
Cartosphere::TriangularMesh::load(const string& path)
{
	if (_bLoadSuccess || _bParseSuccess) clear();

	// Opens given path
	ifstream ifs(path);
	if (!ifs.is_open())
	{
		string message;
		{
			stringstream sst;
			sst << "Could not load path " << path;
			message = sst.str();
		}
		_vInfo.push_back(message);
		return isReady();
	}

	// Temporary state variables
	size_t lineNumber = 0;
	size_t lineParsed = 0;
	vector<size_t> specs;
	double coords[4];
	// Temporary state flags
	size_t format = 0;

	// Start parsing file line by line
	string line;
	istringstream iss;
	while (std::getline(ifs, line))
	{
		// Increment line number and skip empty lines
		++lineNumber;
		if (!line.empty() && line.back() == '\r') { line.pop_back(); }
		if (!line.length()) continue;
		iss.str(line);

		// Skip comment lines and strip inline comments
		if (line[0] == '#') continue;
		auto indexofPound = line.find_first_of('#');
		if (indexofPound != string::npos)
		{
			line.erase(indexofPound);
		}

		// Parse sanitized inputs
		if (lineParsed == 0)
		{
			// Section 0: Size specifications
			size_t size;

			iss >> size;
			if (iss.fail() || size == 0)
			{
				string message;
				{
					stringstream sst;
					sst << "Error in Line " << lineNumber
						<< ": Number of points is missing or zero";
					message = sst.str();
				}
				_vInfo.push_back(message);
				return isReady();
			}
			specs.push_back(size);

			iss >> size;
			if (iss.fail() || size == 0)
			{
				string message;
				{
					stringstream sst;
					sst << "Error in Line " << lineNumber
						<< ": Number of edges is missing or zero";
					message = sst.str();
				}
				_vInfo.push_back(message);
				return isReady();
			}
			specs.push_back(size);

			iss >> size;
			if (iss.fail() || size == 0)
			{
				string message;
				{
					stringstream sst;
					sst << "Error in Line " << lineNumber
						<< ": Number of triangles is missing or zero";
					message = sst.str();
				}
				_vInfo.push_back(message);
				return isReady();
			}
			specs.push_back(size);

			iss >> format;
			if (iss.fail())
			{
				format = 0;
			}
			else if (format != 0)
			{
				string message;
				{
					stringstream sst;
					sst << "Error in Line " << lineNumber
						<< ": File format ID " << format << " is unrecognized";
					message = sst.str();
				}
				_vInfo.push_back(message);
				return isReady();
			}
			specs.push_back(format);
		}
		else if (lineParsed <= specs[0])
		{
			// Section 1: List of points
			for (size_t pos = 0; pos < 2; ++pos)
			{
				iss >> coords[pos];
				if (iss.fail())
				{
					string message;
					{
						stringstream sst;
						sst << "Error in Line " << lineNumber
							<< ": Missing coordinate " << pos;
						message = sst.str();
					}
					_vInfo.push_back(message);
					return isReady();
				}
			}

			iss >> coords[2];
			if (iss.fail())
			{
				// Spherical coordinates in degrees
				Preimage pi(cs_deg2rad(coords[0]), cs_deg2rad(coords[1]));
				_V.emplace_back(pi);
			}
			else
			{
				// Cartesian coordinates in degrees
				Image im(coords[0], coords[1], coords[2]);
				_V.emplace_back(im);
			}

			iss >> coords[3];
			if (!iss.fail())
			{
				string message;
				{
					stringstream sst;
					sst << "Warning in Line " << lineNumber
						<< ": Extra arguments are dropped";
					message = sst.str();
				}
				_vInfo.push_back(message);
			}
		}
		else if (lineParsed <= specs[0] + specs[1])
		{
			// Section 2: List of edges
			UndirectedEdge edge;
			iss >> edge.first;
			iss >> edge.second;
			if (iss.fail())
			{
				string message;
				{
					stringstream sst;
					sst << "Error in Line " << lineNumber
						<< ": Edge specification missing point(s) ";
					message = sst.str();
				}
				_vInfo.push_back(message);
				return isReady();
			}
			_E.push_back(edge);

			iss >> edge.first;
			if (!iss.fail())
			{
				string message;
				{
					stringstream sst;
					sst << "Warning in Line " << lineNumber
						<< ": Extra arguments are dropped";
					message = sst.str();
				}
				_vInfo.push_back(message);
			}
		}
		else if (lineParsed <= specs[0] + specs[1] + specs[2])
		{
			// Section 3: List of triangles
			size_t indices[3];
			char orientation[3] = { '\0', '\0', '\0' };

			for (size_t pos = 0; pos < 3; ++pos)
			{
				iss >> orientation[pos];
				if (iss.fail() || orientation[0] == '\0')
				{
					string message;
					{
						stringstream sst;
						sst << "Error in Line " << lineNumber
							<< ": Argument " << pos
							<< " is missing an orientation";
						message = sst.str();
					}
					_vInfo.push_back(message);
				}
				iss >> indices[pos];
				if (iss.fail() || orientation[0] == '\0')
				{
					string message;
					{
						stringstream sst;
						sst << "Error in Line " << lineNumber
							<< ": Argument " << pos
							<< " is not formatted correctly";
						message = sst.str();
					}
					_vInfo.push_back(message);
				}
			}

			_F.emplace_back(
				std::make_pair(indices[0], orientation[0] == '+'),
				std::make_pair(indices[1], orientation[1] == '+'),
				std::make_pair(indices[2], orientation[2] == '+'));

			iss >> orientation[0];
			if (!iss.fail())
			{
				string message;
				{
					stringstream sst;
					sst << "Warning in Line " << lineNumber
						<< ": Extra arguments are dropped";
					message = sst.str();
				}
				_vInfo.push_back(message);
			}
		}

		++lineParsed;
		iss.clear();
	}

	_bLoadSuccess = true;
	_populate();
	return isReady();
}

bool
Cartosphere::TriangularMesh::save(const string& path) const
{
	ofstream ofs(path);

	if (!ofs.is_open())
	{
		return false;
	}

	ofs << "#Cartosphere Mesh Fomat\n"
		<< "#V #E #F\n"
		<< _V.size() << " "
		<< _E.size() << " "
		<< _F.size() << "\n\n"
		<< "#V List\n";

	for (auto& point : _V)
	{
		ofs << cs_rad2deg(point.p()) << " "
			<< cs_rad2deg(point.a()) << "\n";
	}

	ofs << "\n"
		<< "#E List\n";

	for (auto& edge : _E)
	{
		ofs << edge.first << " " << edge.second << "\n";
	}

	ofs << "\n"
		<< "#F List\n";

	for (auto& triangle : _F)
	{
		auto& a = std::get<0>(triangle);
		auto& b = std::get<1>(triangle);
		auto& c = std::get<2>(triangle);

		ofs << (a.second ? "+" : "-") << a.first << " "
			<< (b.second ? "+" : "-") << b.first << " "
			<< (c.second ? "+" : "-") << c.first << "\n";
	}

	return true;
}

bool
Cartosphere::TriangularMesh::format(const string& path,
	const vector<double>& values) const
{
	// Summary of Logic:
	//   1. Mark all used edges
	//   2. Generate UV sphere (material globe)
	//   3. Generate Segmented Edges (material segment)
	//   4. Generate triangulated faces (material color) if values are provided
	//      - The entire mesh is refined
	//   5. Format Wavefront OBJ output file

	// Export to specified path
	ofstream ofs(path);
	if (!ofs.is_open()) return false;

	// Mark all edges that are part of some triangle
	vector<bool> edgeUsages(_E.size(), false);
	for (auto& triangle : _F)
	{
		edgeUsages[std::get<0>(triangle).first] = true;
		edgeUsages[std::get<1>(triangle).first] = true;
		edgeUsages[std::get<2>(triangle).first] = true;
	}

	// Initialize container for obj vertices and smoothing groups
	vector<Image> vs;
	vector<int> cs;
	vector<vector<vector<size_t>>> sgs;
	vector<string> materials;
	vector<size_t> list;

	// [Color] Subdivide [0,1] into 256 colors
	vector<double> table;
	if (!values.empty())
	{
		table.resize(256);
		for (size_t k = 0; k < table.size(); ++k)
		{
			table[k] = double(1) * k / table.size();
		}
	}

	// Push vertices and edges for a globe
	{
		// Preparation for vertices at certain UV detail level
		const size_t uv = 64;
		const double radius = 0.999;

		// Vertex: north pole
		vs.emplace_back(0, 0, radius);

		// Vertex: all points but the poles
		double x, y, z, a, p;
		for (size_t k = 1; k < uv; ++k)
		{
			p = M_PI * k / uv;
			z = radius * cos(p);
			for (size_t j = 0; j < uv; ++j)
			{
				x = y = radius * sin(p);
				a = 2 * M_PI * j / uv;
				x *= cos(a);
				y *= sin(a);
				vs.emplace_back(x, y, z);
			}
		}
		// Vertex: south pole
		vs.emplace_back(0, 0, -radius);

		// Preparation for faces and smoothing group
		materials.emplace_back("globe");
		sgs.emplace_back();
		auto& sg = sgs.back();

		// Face: cap at north pole
		for (size_t j = 0; j < uv; ++j)
		{
			list = {
				1,
				2 + j,
				2 + (j + 1) % uv
			};
			sg.push_back(list);
		}

		// Face: quad strips
		for (size_t k = 1; k < uv - 1; ++k)
		{
			for (size_t j = 0; j < uv; ++j)
			{
				list = {
					2 + uv * (k - 1) + j,
					2 + uv * k + j,
					2 + uv * k + (j + 1) % uv,
					2 + uv * (k - 1) + (j + 1) % uv
				};
				sg.push_back(list);
			}
		}

		// Face: cap at south pole
		for (size_t j = 0; j < uv; ++j)
		{
			list = {
				2 + uv * (uv - 2) + j,
				2 + uv * (uv - 1),
				2 + uv * (uv - 2) + (j + 1) % uv
			};
			sg.push_back(list);
		}
	}

	// Push vertices and edges for each arc used
	{
		// Desired length and width of each segment
		const double length = 0.1;
		const double width = 0.001;
		const double radius = 1.001;

		// Loop through all edges
		for (size_t i = 0; i < _E.size(); ++i)
		{
			// Skip edge if not used
			if (!edgeUsages[i]) continue;

			// Construct minor arc
			auto& e = _E[i];
			auto& a = _V[e.first];
			auto& b = _V[e.second];
			Arc arc(a, b);

			// Calculate optimal number of segments to divide into
			double span = arc.span();
			size_t segments =
				static_cast<size_t>(std::ceil(span / length));

			// Generate vertices
			size_t offset = vs.size();
			for (size_t s = 0; s <= segments; ++s)
			{
				double u = span * (1.0 * s / segments);
				vs.push_back(arc.local(u, -width) * radius);
				vs.push_back(arc.local(u, width) * radius);
			}

			// Each edge starts its own smoothing group
			materials.emplace_back("segment");
			sgs.emplace_back();
			auto& sg = sgs.back();

			// Generate strips
			for (size_t s = 0; s < segments; ++s)
			{
				list = {
					offset + 2 * s + 1,
					offset + 2 * s + 3,
					offset + 2 * s + 2
				};
				sg.push_back(list);
				list = {
					offset + 2 * s + 2,
					offset + 2 * s + 3,
					offset + 2 * s + 4
				};
				sg.push_back(list);
			}
		}
	}

	cs.resize(vs.size());

	/////////////////////////////////////
	// Outputs OBJ File Comment Header //
	/////////////////////////////////////

	// Header information
	ofs << "# Wavefront OBJ File Format\n"
		<< "# This file is generated by Cartosphere\n";
	
	// Outputs V, N, T count
	ofs << "# .OBJ Vertex: " << vs.size() << "\n"
		<< "# .OBJ Normal: " << vs.size() << "\n";

	size_t polygonCount = 0;
	for (auto& sg : sgs)
	{
		polygonCount += sg.size();
	}
	ofs << "# .OBJ Polygon: " << polygonCount << "\n";

	// [Color] Outputs Vt count
	if (!values.empty())
	{
		ofs << "# .OBJ Texture Coordinates: " << cs.size() << "\n";
	}

	// Outputs original mesh info
	auto stat = statistics();
	ofs << "# Mesh Vertex: " << stat.V << "\n"
		<< "# Mesh Edges: " << stat.E << "\n"
		<< "# Mesh Faces: " << stat.F << "\n";

	// Directive for the mesh
	ofs << "\n"
		<< "# The file cartosphere.mtl must exist in the same folder\n"
		<< "mtllib cartosphere.mtl\n";

	// Print all vertices
	ofs << "\n";
	for (auto& v : vs)
	{
		// A normal vector for a point v on the unit sphere is equal to v
		ofs << "v  " << v.x << " " << v.y << " " << v.z << "\n";
	}

	// Print all normal vectors
	ofs << "\n";
	for (auto& v : vs)
	{
		// A normal vector for a point v on the unit sphere is equal to v
		auto vn = v.to_unit_vector();
		ofs << "vn " << vn.x << " " << vn.y << " " << vn.z << "\n";
	}

	// [Color] Print 256-level texture coordinates
	if (!values.empty())
	{
		ofs << "\n";
		for (auto& c : table)
		{
			ofs << "vt " << c << " " << c << "\n";
		}
	}

	// Print all smoothing groups
	for (size_t i = 0; i < sgs.size(); ++i)
	{
		// Start a smoothing group
		ofs << "\n";
		ofs << "s " << i + 1 << "\n"
			<< "usemtl " << materials[i] << "\n";

		// Output all polygons in this smoothing group
		const auto& sg = sgs[i];
		for (auto& f : sg)
		{
			ofs << "f";

			for (auto& v : f)
			{
				ofs << " " << v << "/";
				// [Color] Print 256-level color for the vertex
				if (cs[v - 1] != 0)
				{
					ofs << cs[v - 1];
				}
				ofs << "/" << v;
			}
			ofs << "\n";
		}

		ofs << "s off\n";
	}

	return true;
}

bool
Cartosphere::TriangularMesh::formatPoly(
	const string& path, const vector<double>& values) const
{
	// Summary of Logic:
	//   1. Generate polygonal faces
	//   2. Format Wavefront OBJ output file

	// Export to specified path
	ofstream ofs(path);
	if (!ofs.is_open()) return false;

	// Initialize container for obj vertices and smoothing groups
	vector<Image> vs;
	vector<size_t> vcs;
	vector<vector<vector<size_t>>> sgs;
	vector<string> materials;
	vector<size_t> list;

	// [Color] Subdivide [0,1] into 256 colors
	vector<double> cs;
	if (!values.empty())
	{
		cs.resize(256);
		for (size_t k = 0; k < cs.size(); ++k)
		{
			cs[k] = double(1) * k / (cs.size());
		}
	}

	// Quantify the range
	double min = *std::min_element(values.cbegin(), values.cend());
	double max = *std::max_element(values.cbegin(), values.cend());
	double range = max - min;

	// Push new smoothing group and new material for all mesh triangles
	sgs.emplace_back();
	materials.push_back("color");

	// Push vertices
	auto& sg = sgs.back();
	for (auto& fvs : _FV)
	{
		sg.emplace_back();
		auto& s = sg.back();
		for (auto& fv : fvs)
		{
			s.push_back(vs.size() + 1);
			vs.push_back(_V[fv].image());
			vcs.push_back(size_t(1 + double(255) * (values[fv] - min) / range));
		}
	}

	// Header information
	ofs << "# Wavefront OBJ File Format\n"
		<< "# This file is generated by Cartosphere\n";

	// Outputs V, N, T count
	ofs << "# .OBJ Vertex: " << vs.size() << "\n"
		<< "# .OBJ Normal: " << vs.size() << "\n";

	size_t polygonCount = 0;
	for (auto& sg : sgs)
	{
		polygonCount += sg.size();
	}
	ofs << "# .OBJ Polygon: " << polygonCount << "\n";

	// [Color] Outputs Vt count
	if (!values.empty())
	{
		ofs << "# .OBJ Texture Coordinates: " << cs.size() << "\n";
	}

	// Outputs original mesh info
	auto stat = statistics();
	ofs << "# Mesh Vertex: " << stat.V << "\n"
		<< "# Mesh Edges: " << stat.E << "\n"
		<< "# Mesh Faces: " << stat.F << "\n";

	// Directive for the mesh
	ofs << "\n"
		<< "# The file cartosphere.mtl must exist in the same folder\n"
		<< "mtllib cartosphere.mtl\n";

	// Print all vertices
	ofs << "\n";
	for (auto& v : vs)
	{
		// A normal vector for a point v on the unit sphere is equal to v
		ofs << "v  " << v.x << " " << v.y << " " << v.z << "\n";
	}

	// Print all normal vectors
	ofs << "\n";
	for (auto& v : vs)
	{
		// A normal vector for a point v on the unit sphere is equal to v
		ofs << "vn " << v.x << " " << v.y << " " << v.z << "\n";
	}

	// [Color] Print 256-level texture coordinates
	if (!values.empty())
	{
		ofs << "\n";
		for (auto& c : cs)
		{
			ofs << "vt " << c << " " << c << "\n";
		}
	}

	// Print all smoothing groups
	for (size_t i = 0; i < sgs.size(); ++i)
	{
		// Start a smoothing group
		ofs << "\n";
		ofs << "s " << i + 1 << "\n"
			<< "usemtl " << materials[i] << "\n";

		// Output all polygons in this smoothing group
		const auto& sg = sgs[i];
		for (auto& f : sg)
		{
			ofs << "f";

			for (auto& v : f)
			{
				ofs << " " << v << "/";
				// [Color] Print 256-level color for the vertex
				if (vcs[v - 1] != 0)
				{
					ofs << vcs[v - 1];
				}
				ofs << "/" << v;
			}
			ofs << "\n";
		}
	}

	return true;
}

void
Cartosphere::TriangularMesh::refine()
{
	// Update VEF count, as a special case with division set to 2
	size_t V = _V.size() + _E.size();
	size_t E = 2 * _E.size() + 3 * _F.size();
	size_t F = 4 * _F.size();

	auto edges = std::move(_E);
	auto triangles = std::move(_F);

	_V.reserve(V);
	_E.reserve(E);
	_F.reserve(F);

	// Each old edge will produce one new midpoint
	for (auto& pair : edges)
	{
		const Point& A = _V[pair.first];
		const Point& B = _V[pair.second];

		size_t midpointIndex = _V.size();
		_V.push_back(midpoint(A, B));

		// Each new midpoint will bisect the old edge into two new edges
		_E.emplace_back(std::make_pair(pair.first, midpointIndex));
		_E.emplace_back(std::make_pair(midpointIndex, pair.second));
	}

	// Assemble new list of triangles
	size_t myMidpoints[3];
	vector<DirectedEdge> myEdges;
	vector<DirectedEdgeTriplet> myTriangles;
	myEdges.reserve(12);
	myTriangles.reserve(4);
	for (auto& triangle : triangles)
	{
		auto& a = std::get<0>(triangle);
		auto& b = std::get<1>(triangle);
		auto& c = std::get<2>(triangle);
		// Push 6 constructed edges and retrieve midpoints
		myEdges.emplace_back(2 * a.first, a.second);
		myEdges.emplace_back(2 * a.first + 1, a.second);
		if (!a.second)
			std::swap(myEdges[0], myEdges[1]);

		myEdges.emplace_back(2 * b.first, b.second);
		myEdges.emplace_back(2 * b.first + 1, b.second);
		if (!b.second)
			std::swap(myEdges[2], myEdges[3]);

		myEdges.emplace_back(2 * c.first, c.second);
		myEdges.emplace_back(2 * c.first + 1, c.second);
		if (!c.second)
			std::swap(myEdges[4], myEdges[5]);

		// Retrieve the midpoints
		if (myEdges[0].second)
			myMidpoints[0] = _E[myEdges[0].first].second;
		else
			myMidpoints[0] = _E[myEdges[0].first].first;

		if (myEdges[2].second)
			myMidpoints[1] = _E[myEdges[2].first].second;
		else
			myMidpoints[1] = _E[myEdges[2].first].first;

		if (myEdges[4].second)
			myMidpoints[2] = _E[myEdges[4].first].second;
		else
			myMidpoints[2] = _E[myEdges[4].first].first;

		// Push six new edges from the three new midpoints per triangle
		myEdges.emplace_back(_E.size(), true);
		myEdges.emplace_back(_E.size(), false);
		_E.push_back(std::make_pair(myMidpoints[0], myMidpoints[2]));

		myEdges.emplace_back(_E.size(), true);
		myEdges.emplace_back(_E.size(), false);
		_E.push_back(std::make_pair(myMidpoints[1], myMidpoints[0]));

		myEdges.emplace_back(_E.size(), true);
		myEdges.emplace_back(_E.size(), false);
		_E.push_back(std::make_pair(myMidpoints[2], myMidpoints[1]));

		// Construct four new triangles per old triangle
		_F.emplace_back(myEdges[0], myEdges[6], myEdges[5]);
		_F.emplace_back(myEdges[1], myEdges[2], myEdges[8]);
		_F.emplace_back(myEdges[10], myEdges[3], myEdges[4]);
		_F.emplace_back(myEdges[7], myEdges[9], myEdges[11]);

		myEdges.clear();
		myTriangles.clear();
	}

	// Fill the simplices
	_populate();
}

void
Cartosphere::TriangularMesh::refine(size_t division)
{
	// If division is 0 or 1, quit
	// If division is 2, delegate to the mid-point refinement
	if (division < 2)
	{
		return;
	}
	else if (division == 2)
	{
		return refine();
	}

	// Update VEF count: original (V', E', F') -> refined (V, E, F)
	// V = V' + (d-1)E' + (d-1)(d-2)/2
	// E = d*E' + 3F'd(d-1)/2
	// F = ddF';
	size_t V = _V.size()
		+ (division - 1) * _E.size()
		+ (division - 1) * (division - 2) / 2;
	size_t E = division * _E.size()
		+ 3 * _F.size() * division * (division - 1) / 2;
	size_t F = division * division * _F.size();

	// Preserve edges and triangles
	auto edges = std::move(_E);
	auto triangles = std::move(_F);

	// Reserve (perhaps more) memory
	_V.reserve(V);
	_E.reserve(E);
	_F.reserve(F);

	// Each old edge will produce (d-1) new midpoints and d new edges
	for (auto& pair : edges)
	{
		const Point& A = _V[pair.first];
		const Point& B = _V[pair.second];

		// Add first edge
		size_t midpointIndex = _V.size();
		_E.emplace_back(std::make_pair(pair.first, midpointIndex));

		// Construct midpoints and their in-between edges
		Cartosphere::Arc arc(A, B);
		for (size_t d = 1; d < division; ++d, ++midpointIndex)
		{
			double alpha = 1.0 * d / division;
			_V.push_back(arc.local(alpha));
			_E.push_back(std::make_pair(midpointIndex, midpointIndex + 1));
		}

		// Add last edge
		_E.emplace_back(std::make_pair(midpointIndex - 1, pair.second));
	}

	// TODO: construct vertices and edges in the interior of the triangle

	// TODO: construct faces

	// Refresh the list of simplices
	_populate();
}

void
Cartosphere::TriangularMesh::fill(SparseMatrixRowMajor& A, Triangle::Integrator intr) const
{
	auto stat = statistics();

	// Construct a square matrix and one row for each vertex
	int N = (int)stat.V;
	A.resize(N, N);

	// Preparation!
	// 1. Need to get the poles (E-loop)
	vector<Cartosphere::Point> poles;
	poles.reserve(stat.E);

	// E-loop
	for (const auto& e : _E)
	{
		// Construct directional arc and extract its pole
		const Point& a = _V[e.first];
		const Point& b = _V[e.second];
		Arc arc(a, b);
		poles.push_back(arc.pole());
	}

	// 2. Need to get the magnitude of all gradient vectors (T-loop)
	vector<double> magnitudes;
	magnitudes.reserve(3 * stat.F);
	Point pole, vertex;

	// F-loop
	for (size_t k = 0; k < stat.F; ++k)
	{
		// Vertex A: PI/2 minus distance from A to the pole of BC
		vertex = _vt[k].A;
		{
			const auto& edgeInfo = std::get<1>(_F[k]);
			const auto& edge = _E[edgeInfo.first];
			pole = poles[edgeInfo.first];
			if (!edgeInfo.second)
				pole.flip();
		}
		magnitudes.push_back(M_PI_2 - distance(pole, vertex));
		// Vertex B: PI/2 minus distance from B to the pole of CA
		vertex = _vt[k].B;
		{
			const auto& edgeInfo = std::get<2>(_F[k]);
			const auto& edge = _E[edgeInfo.first];
			pole = poles[edgeInfo.first];
			if (!edgeInfo.second)
				pole.flip();
		}
		magnitudes.push_back(M_PI_2 - distance(pole, vertex));
		// Vertex C: PI/2 minus distance from C to the pole of AB
		vertex = _vt[k].C;
		{
			const auto& edgeInfo = std::get<0>(_F[k]);
			const auto& edge = _E[edgeInfo.first];
			pole = poles[edgeInfo.first];
			if (!edgeInfo.second)
				pole.flip();
		}
		magnitudes.push_back(M_PI_2 - distance(pole, vertex));
	}

	for (size_t k = 0; k < 3 * stat.F; ++k)
	{
		magnitudes[k] = 1 / magnitudes[k];
	}

	// 3. Need to numerically construct the local stiffness matrices
	// The inner product of gradient vectors has been converted to a spherical
	// trigonometry problem, but we need to use quadratures on spherical triangles.
	auto L = new double [stat.F][3][3]();

	// T-loop.
	for (size_t k = 0; k < stat.F; ++k)
	{
		auto& local = L[k];
		const auto& triangle = _vt[k];

		// Compute the diagonal entries
		// double area = triangle.area();
		// local[0][0] = area * pow(magnitudes[3 * k + 0], 2);
		// local[1][1] = area * pow(magnitudes[3 * k + 1], 2);
		// local[2][2] = area * pow(magnitudes[3 * k + 2], 2);
		// Code above is bad because the exact solutions are inconsistent with
		// the numerical integration used on the off-diagonal entries

		// Extract poles of edges for use of the equivalent geometric problem
		auto poleBC = poles[std::get<1>(_F[k]).first];
		if (!std::get<1>(_F[k]).second)
			poleBC.flip();
		auto poleCA = poles[std::get<2>(_F[k]).first];
		if (!std::get<2>(_F[k]).second)
			poleCA.flip();
		auto poleAB = poles[std::get<0>(_F[k]).first];
		if (!std::get<0>(_F[k]).second)
			poleAB.flip();
		
		// Numerically integrate the inner product of gradient of functions
		auto unit = [](const Point& x)->double { return 1; };
		local[0][0] = 
			triangle.integrate(unit, intr) * magnitudes[3 * k + 0] * magnitudes[3 * k + 0];

		local[1][1] =
			triangle.integrate(unit, intr) * magnitudes[3 * k + 1] * magnitudes[3 * k + 1];

		local[2][2] =
			triangle.integrate(unit, intr) * magnitudes[3 * k + 2] * magnitudes[3 * k + 2];

		// Off-diagonal entries
		auto iAB = [&poleBC, &poleCA](const Point &x)->double {
			return cos(angle(poleCA.image(), x.image(), poleBC.image()));
		};
		local[1][0] = local[0][1] =
			triangle.integrate(iAB, intr) * magnitudes[3 * k + 0] * magnitudes[3 * k + 1];

		auto iCA = [&poleBC, &poleAB](const Point& x)->double {
			return cos(angle(poleBC.image(), x.image(), poleAB.image()));
		};
		local[2][0] = local[0][2] =
			triangle.integrate(iCA, intr) * magnitudes[3 * k + 0] * magnitudes[3 * k + 2];

		auto iBC = [&poleAB, &poleCA](const Point& x)->double {
			return cos(angle(poleAB.image(), x.image(), poleCA.image()));
		};
		local[2][1] = local[1][2] =
			triangle.integrate(iBC, intr) * magnitudes[3 * k + 1] * magnitudes[3 * k + 2];
	}

	// 4. Calculate the adjacency of vertices from the list of undirected edges
	// by populating the zero-valued entries of the global stiffness matrix
	vector<size_t> rows, cols;
	
	// V-loop: diagonal entries
	for (size_t i = 0; i < stat.V; ++i)
	{
		rows.push_back(i);
		cols.push_back(i);
	}
	
	// E-loop: undirected edges gives two directed edges
	for (size_t i = 0; i < stat.E; ++i)
	{
		rows.push_back(_E[i].first);
		cols.push_back(_E[i].second);
		rows.push_back(_E[i].second);
		cols.push_back(_E[i].first);
	}

	// 5. Assemble the global stiffness matrix from the local stiffness matrix
	vector<SparseMatrixEntry> entries;
	for (size_t k = 0; k < rows.size(); ++k)
	{
		double value = 0;
		
		// Calculate the common support
		const auto& support1 = _VF[rows[k]];
		const auto& support2 = _VF[cols[k]];
		vector<size_t> support;
		std::set_intersection(
			support1.cbegin(), support1.cend(),
			support2.cbegin(), support2.cend(),
			std::back_inserter(support)
		);

		// For each face in the support find the appropriate local stiffness
		for (auto index : support)
		{
			const auto& fv = _FV[index];
			size_t i = std::find(fv.cbegin(), fv.cend(), rows[k]) - fv.cbegin();
			size_t j = std::find(fv.cbegin(), fv.cend(), cols[k]) - fv.cbegin();
			value += L[index][i][j];
		}
		
		SparseMatrixEntry e;
		double v = e.value();
		// Unfortunately, the value() field is private.
		entries.emplace_back((int)rows[k], (int)cols[k], value);
	}

	delete[] L;

	A.setFromTriplets(entries.begin(), entries.end());
}

void
Cartosphere::TriangularMesh::fill(SparseMatrixRowMajor& A, SparseMatrixRowMajor& M, Triangle::Integrator intr) const
{
	// 1. Build the gradient inner matrix
	fill(A, intr);

	// 2. Build the inner matrix
	auto stat = statistics();

	// Construct a square matrix and one row for each vertex
	int N = (int)stat.V;
	M.resize(N, N);

	// 3. Need to numerically construct the local stiffness matrices
	auto L = new double[stat.F][3][3]();

	// T-loop.
	for (size_t k = 0; k < stat.F; ++k)
	{
		auto& local = L[k];
		const auto& triangle = _vt[k];

		for (int i = 0; i < 3; ++i)
		{
			for (int j = 0; j < 3; ++j)
			{
				Function f = triangle.element(i);
				Function g = triangle.element(j);
				Function inner = [&f, &g](const Point& p) -> double {
					return f(p) * g(p);
				};
				L[k][i][j] = triangle.integrate(inner, intr);
			}
		}
	}

	// 4. Calculate the adjacency of vertices from the list of undirected edges
	// by populating the zero-valued entries of the global stiffness matrix
	vector<size_t> rows, cols;

	// V-loop: diagonal entries
	for (size_t i = 0; i < stat.V; ++i)
	{
		rows.push_back(i);
		cols.push_back(i);
	}

	// E-loop: undirected edges gives two directed edges
	for (size_t i = 0; i < stat.E; ++i)
	{
		rows.push_back(_E[i].first);
		cols.push_back(_E[i].second);
		rows.push_back(_E[i].second);
		cols.push_back(_E[i].first);
	}

	// 5. Assemble the global stiffness matrix from the local stiffness matrix
	vector<SparseMatrixEntry> entries;
	for (size_t k = 0; k < rows.size(); ++k)
	{
		double value = 0;

		// Calculate the common support
		const auto& support1 = _VF[rows[k]];
		const auto& support2 = _VF[cols[k]];
		vector<size_t> support;
		std::set_intersection(
			support1.cbegin(), support1.cend(),
			support2.cbegin(), support2.cend(),
			std::back_inserter(support)
		);

		// For each face in the support find the appropriate local stiffness
		for (auto index : support)
		{
			const auto& fv = _FV[index];
			size_t i = std::find(fv.cbegin(), fv.cend(), rows[k]) - fv.cbegin();
			size_t j = std::find(fv.cbegin(), fv.cend(), cols[k]) - fv.cbegin();
			value += L[index][i][j];
		}

		SparseMatrixEntry e;
		double v = e.value();
		// Unfortunately, the value() field is private.
		entries.emplace_back((int)rows[k], (int)cols[k], value);
	}

	delete[] L;

	M.setFromTriplets(entries.begin(), entries.end());
}

void
Cartosphere::TriangularMesh::fill(ColVector& b, Function f,
	Triangle::Integrator intr) const
{
	Stats stat = statistics();
	
	b.resize(stat.V);
	// Loop through all vertices
	for (size_t i = 0; i < stat.V; ++i)
	{
		b[i] = 0;
		// Obtain the precalculated faces that share each vertex
		const auto &star = _VF[i];
		for (size_t k = 0; k < star.size(); ++k)
		{
			// For each face, obtain the correct element
			size_t vid = std::find(_FV[star[k]].cbegin(), _FV[star[k]].cend(), i) - _FV[star[k]].cbegin();
			auto g = _vt[star[k]].element(vid);
			auto h = [f, g](const Point& p)->double { return f(p) * g(p); };
			b[i] += _vt[star[k]].integrate(h, intr);

			// Debug
			// std::cout
			// 	<< " V=" << i
			// 	<< " nb=" << star[k]
			// 	<< " 1@" << vid
			// 	<< " I=" << _vt[star[k]].integrate(h, intr) << "\n";
		}
		
		// Debug
		// std::cout << "F_" << i << " = " << b[i] << "\n";
	}
}

void
Cartosphere::TriangularMesh::reportAreas()
{
	double area = 0;
	double totalArea = 0;

	for (auto& triangle : _vt)
	{
		area = triangle.areaEuclidean();
		totalArea += area;
		string message;
		{
			auto& A = triangle.A.image();
			auto& B = triangle.B.image();
			auto& C = triangle.C.image();
			stringstream sst;
			sst << "Area: " << area
				<< " | A(" << A.x << ", " << A.y << ", " << A.z << ") "
				<< " B(" << B.x << ", " << B.y << ", " << B.z << ") "
				<< " C(" << C.x << ", " << C.y << ", " << C.z << ") ";
			message = sst.str();
		}
		_vInfo.push_back(message);
	}

	string message;
	{
		stringstream sst;
		sst << "Total Area: " << totalArea;
		message = sst.str();
	}
	_vInfo.push_back(message);
}

double
Cartosphere::TriangularMesh::interpolate(const Point& p) const
{
	size_t i = _lookup(p);
	FL3 c = _vt[i].barycentric(p);
	FL3 v(_a[_FV[i][0]], _a[_FV[i][1]], _a[_FV[i][2]]);
	double value = dot(c, v);
	return value;
}

FL3
Cartosphere::TriangularMesh::gradient(const Point& p) const
{
	size_t i = _lookup(p);

	FL3 u, v, w;
	if (false)
	{
		u = transport(_V[_FV[i][0]], p, _vt[i].gradient(0));
		v = transport(_V[_FV[i][1]], p, _vt[i].gradient(1));
		w = transport(_V[_FV[i][2]], p, _vt[i].gradient(2));

		if (u.anynan())
		{
			u = _vt[i].gradient(0);
		}
		if (v.anynan())
		{
			v = _vt[i].gradient(1);
		}
		if (w.anynan())
		{
			w = _vt[i].gradient(2);
		}
	}
	else
	{
		u = _vt[i].gradient(0, p);
		v = _vt[i].gradient(1, p);
		w = _vt[i].gradient(2, p);
		// std::cout << "<" << u << "> <" << v << "> <" << w << "\n";
	}

	return _a[_FV[i][0]] * u + _a[_FV[i][1]] * v + _a[_FV[i][2]] * w;
}

void
Cartosphere::TriangularMesh::set(const vector<double>& values)
{
	_a = values;
	// Initialize an empty gradient field, one vector for each vertex
	_grad.assign(_V.size(), FL3());
	// Loop through all the faces
	for (int k = 0; k < _vt.size(); ++k)
	{
		// Face
		const auto& f = _vt[k];
		// Indices of vertices
		const auto& v = _FV[k];
		// Obtain a gradient vector for each vertex.
		// Scale accordingly by the values.
		// Accumulate per vertex.
		for (int i = 0; i < 3; ++i)
		{
			size_t j = v[i];
			_grad[j] += values[j] * f.gradient(i);
		}
	}
}

double
Cartosphere::TriangularMesh::integrate(const Function& f,
	Quadrature rule, Triangle::Integrator intr) const
{
	double result = 0;
	switch (rule)
	{
	case Quadrature::AreaWeighted:
	{
		for (auto& triangle : _vt)
		{
			result += triangle.integrate(f, intr);
		}
	} break;
	default:
		break;
	}
	return result;
}

Cartosphere::TriangularMesh::Stats
Cartosphere::TriangularMesh::statistics() const
{
	Stats s;

	// Report basic entity count
	s.V = _V.size();
	s.E = _E.size();
	s.F = _F.size();

	// Report element statistics
	s.areaElementMax = DoubleMinimum;
	s.areaElementMin = DoubleMaximum;
	s.diameterElementMax = DoubleMinimum;

	for (auto& triangle : _vt)
	{
		double area = triangle.area();
		s.areaElementMax = std::max(s.areaElementMax, area);
		s.areaElementMin = std::min(s.areaElementMin, area);

		double diameter = triangle.diameter();
		s.diameterElementMax = std::max(s.diameterElementMax, diameter);
	}
	s.areaElementDisparity = s.areaElementMax / s.areaElementMin;

	return s;
}

void
Cartosphere::TriangularMesh::_populate()
{
	// Clear redundant data to regenerate them next
	_vt.clear();
	_VE.clear();
	_VF.clear();
	_FV.clear();

	// Populate triangles
	size_t pointIndex[6];
	_FV.resize(_F.size());
	for (size_t index = 0; index < _F.size(); ++index)
	{
		auto& triplet = _F[index];
		// Extract the 3 vertices with duplication
		pointIndex[0] = _E[std::get<0>(triplet).first].first;
		pointIndex[1] = _E[std::get<0>(triplet).first].second;
		if (!std::get<0>(triplet).second)
			std::swap(pointIndex[0], pointIndex[1]);
		pointIndex[2] = _E[std::get<1>(triplet).first].first;
		pointIndex[3] = _E[std::get<1>(triplet).first].second;
		if (!std::get<1>(triplet).second)
			std::swap(pointIndex[2], pointIndex[3]);
		pointIndex[4] = _E[std::get<2>(triplet).first].first;
		pointIndex[5] = _E[std::get<2>(triplet).first].second;
		if (!std::get<2>(triplet).second)
			std::swap(pointIndex[4], pointIndex[5]);

		// Check if the edges form a simplex
		if (pointIndex[0] == pointIndex[1] || pointIndex[1] != pointIndex[2] ||
			pointIndex[2] == pointIndex[3] || pointIndex[3] != pointIndex[4] ||
			pointIndex[4] == pointIndex[5] || pointIndex[5] != pointIndex[0])
		{
			string message;
			{
				stringstream sst;
				sst << "Error in Face #" << index
					<< ": Edges do not form a valid simplex";
				message = sst.str();
			}
			_vInfo.push_back(message);
			_bParseSuccess = false;
			return;
		}

		// Push triangles that contain redundant information
		_FV[index] = { pointIndex[0], pointIndex[2], pointIndex[4] };
		_vt.emplace_back(
			Point(_V[pointIndex[0]]),
			Point(_V[pointIndex[2]]),
			Point(_V[pointIndex[4]])
		);
	}
	_bParseSuccess = true;

	// Populate the list of edges sharing a vertex
	_VE.resize(_V.size());
	for (size_t k = 0; k < _E.size(); ++k)
	{
		_VE[_E[k].first].push_back(k);
	}

	// Populate the list of faces sharing a vertex
	_VF.resize(_V.size());
	for (size_t k = 0; k < _F.size(); ++k)
	{
		for (size_t i = 0; i < _FV[k].size(); ++i)
		{
			_VF[_FV[k][i]].push_back(k);
		}
	}

	// Construct the fast-lookup structure
	//_tree.build(_vt);
}

void
Cartosphere::TriangularMesh::_gradient(const vector<double>& a)
{
	_grad.resize(_V.size());

	for (int i = 0; i < _FV.size(); ++i)
	{
		vector<size_t> V = _FV[i];
		for (int j = 0; j < 3; ++j)
		{
			_grad[V[j]] += a[i] * _vt[i].gradient(j);
		}
	}
}

size_t
Cartosphere::TriangularMesh::_lookup(const Point& p) const
{
	for (size_t i = 0; i < _vt.size(); ++i)
	{
		if (_vt[i].contains(p))
		{
			return i;
		}
	}
	return (size_t)(-1);
}

FL3
Cartosphere::transport(const Point& from, const Point& to, const FL3& vector)
{
	return Arc(from, to).rotate(vector);
}
