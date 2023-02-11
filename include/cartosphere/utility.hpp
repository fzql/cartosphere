
#ifndef __UTILITY_HPP__
#define __UTILITY_HPP__

// Detect compiler
// Ref:	https://stackoverflow.com/a/5920028/1377770
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
	#define IS_WINDOWS
#elif defined(__APPLE__) || defined(__linux__) || defined(__unix__) || defined(_POSIX_VERSION)
	#define APPLE_LIKE
#else
#   error "Unknown compiler"
#endif

// Set the floating point used in core calculations
using FLP = double;

#include <limits>
static const FLP EPS = std::numeric_limits<FLP>::epsilon();

// Use core math functionality
#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif // !_USE_MATH_DEFINES
#include <cmath>

// Commonly used templates
#include <vector>
using std::vector;

#include <string>
using std::string;

#include <fstream>
using std::ifstream;
using std::ofstream;

#include <sstream>
using std::stringstream;

// Terminal output
#include <iostream>
#include <iomanip>
#include <chrono>
using std::chrono::steady_clock;
using std::chrono::duration_cast;
using std::chrono::milliseconds;

// Eigen classes
#pragma warning(push)
#pragma warning(disable: 4819)
#include <Eigen/Core>
using Vector = Eigen::Matrix<double, Eigen::Dynamic, 1>;
using Eigen::ArrayXd;
using Eigen::VectorXd;
using Eigen::RowVectorXd;
using Eigen::MatrixXd;

#include <Eigen/Dense>
using Matrix = Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;

#include <Eigen/SparseCore>
using CSR_Matrix = Eigen::SparseMatrix<FLP, Eigen::RowMajor>;
using Entry = Eigen::Triplet<FLP>;

#include <Eigen/IterativeLinearSolvers>
using BiCGSTAB_iLUT_Solver = Eigen::BiCGSTAB<CSR_Matrix, Eigen::IncompleteLUT<CSR_Matrix::Scalar>>;
#pragma warning(pop)

// Numerics, algorithms, and functionals
#include <numeric>
#include <algorithm>
#include <functional>

// Character types to support string manipulations
#include <cctype>
#include <locale>

// trim from start (in place)
static inline void ltrim(std::string& s) {
	s.erase(s.begin(),
		std::find_if(s.begin(), s.end(),
		[](unsigned char ch) {
			return !std::isspace(ch);
		})
	);
}

// trim from end (in place)
static inline void rtrim(std::string& s) {
	s.erase(std::find_if(s.rbegin(), s.rend(),
		[](unsigned char ch) {
			return !std::isspace(ch);
		}
	).base(),s.end());
}

// Trim from both ends (in place)
static inline void trim(std::string& s) {
	ltrim(s);
	rtrim(s);
}

// Trim from start (copying)
static inline std::string ltrim_copy(std::string s) {
	ltrim(s);
	return s;
}

// Trim from end (copying)
static inline std::string rtrim_copy(std::string s) {
	rtrim(s);
	return s;
}

// Trim from both ends (copying)
static inline std::string trim_copy(std::string s) {
	trim(s);
	return s;
}

// Switch endian.
// By default, little endian is used on Windows.
// Ref: https://stackoverflow.com/a/3824338
template <class T>
void endswap(T *objp)
{
  unsigned char *memp = reinterpret_cast<unsigned char*>(objp);
  std::reverse(memp, memp + sizeof(T));
}

#endif // !__UTILITY_HPP__
