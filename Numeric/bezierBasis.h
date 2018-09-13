// Gmsh - Copyright (C) 1997-2018 C. Geuzaine, J.-F. Remacle
//
// See the LICENSE.txt file for license information. Please report all
// bugs and problems to the public mailing list <gmsh@onelab.info>.

#ifndef _BEZIER_BASIS_H_
#define _BEZIER_BASIS_H_

#include <vector>
#include "fullMatrix.h"
#include "FuncSpaceData.h"
#include "BasisFactory.h"

class MElement;
class bezierBasisRaiser;

class bezierBasis {
private:
  // the 'numLagCoeff' first exponents are related to 'real' values
  int _numLagCoeff;
  int _numDivisions, _dimSimplex;
  const FuncSpaceData _data;
  bezierBasisRaiser *_raiser;

  friend class bezierBasisRaiser;
  fullMatrix<double> _exponents;

public:
  fullMatrix<double> matrixLag2Bez;
  fullMatrix<double> matrixBez2Lag;
  fullMatrix<double> subDivisor;

  // Constructors
  bezierBasis(FuncSpaceData data);
  ~bezierBasis();

  // get methods
  inline int getDim() const { return _exponents.size2(); }
  inline int getType() const { return _data.elementType(); }
  inline int getOrder() const { return _data.spaceOrder(); }
  inline int getDimSimplex() const { return _dimSimplex; }
  inline int getNumCoeff() const { return _exponents.size1(); }
  inline int getNumLagCoeff() const { return _numLagCoeff; }
  inline int getNumDivision() const { return _numDivisions; }
  inline int getNumSubNodes() const { return subDivisor.size1(); }
  inline FuncSpaceData getFuncSpaceData() const { return _data; }
  bezierBasisRaiser *getRaiser() const;

  // Evaluate Bezier functions at the point (u, v, w)
  void f(double u, double v, double w, double *sf) const;

  // generate Bezier points
  void generateBezierPoints(fullMatrix<double> &) const;

  // transform coeff Lagrange into Bezier coeff
  void lag2Bez(const fullMatrix<double> &lag, fullMatrix<double> &bez) const;

  // Subdivide Bezier coefficients
  void subdivideBezCoeff(const fullMatrix<double> &coeff,
                         fullMatrix<double> &subCoeff) const;
  void subdivideBezCoeff(const fullVector<double> &coeff,
                         fullVector<double> &subCoeff) const;

  // Interpolation of n functions on N points :
  // coeffs(numCoeff, n) and uvw(N, dim)
  // => result(N, n)
  void interpolate(const fullMatrix<double> &coeffs,
                   const fullMatrix<double> &uvw, fullMatrix<double> &result,
                   bool bezCoord = false) const;
  void interpolate(const fullVector<double> &coeffs,
                   const fullMatrix<double> &uvw, fullVector<double> &result,
                   bool bezCoord = false) const
  {
    int size = uvw.size1();
    result.resize(size);
    fullMatrix<double> c(const_cast<double *>(coeffs.getDataPtr()), size, 1);
    fullMatrix<double> r(const_cast<double *>(result.getDataPtr()), size, 1);
    interpolate(c, uvw, r, bezCoord);
  }

private:
  void _construct();
  void _constructPyr();
  void _FEpoints2BezPoints(fullMatrix<double> &) const;
};

class bezierBasisRaiser {
  // Let f, g, h be three function whose Bezier coefficients are given.
  // This class allows to compute the Bezier coefficients of f*g and f*g*h.
private:
  class _Data {
    friend class bezierBasisRaiser;

  private:
    int i, j, k;
    double val;

  public:
    _Data(double vv, int ii, int jj = -1, int kk = -1)
      : i(ii), j(jj), k(kk), val(vv)
    {
    }
  };
  std::vector<std::vector<_Data> > _raiser2, _raiser3;
  const bezierBasis *_bfs;

public:
  bezierBasisRaiser(const bezierBasis *bezier) : _bfs(bezier)
  {
    _fillRaiserData();
  };

  void computeCoeff(const fullVector<double> &coeffA,
                    const fullVector<double> &coeffB,
                    fullVector<double> &coeffSquare);
  void computeCoeff(const fullMatrix<double> &coeffA,
                    const fullMatrix<double> &coeffB,
                    fullMatrix<double> &coeffSquare);
  void computeCoeff(const fullVector<double> &coeffA,
                    const fullVector<double> &coeffB,
                    const fullVector<double> &coeffC,
                    fullVector<double> &coeffCubic);
  void computeCoeff(const fullVector<double> &coeffA,
                    const fullMatrix<double> &coeffB,
                    const fullMatrix<double> &coeffC,
                    fullMatrix<double> &coeffCubic);

private:
  void _fillRaiserData();
  void _fillRaiserDataPyr();
};

class bezierCoeff : public fullMatrix<double> {
  // TODO: test if access would be faster if fullMatrix::operator(int r) was
  // implemented (for fullmatrix with only 1 column)
private :
  FuncSpaceData _data;
  const bezierBasis *_basis;

public:
  bezierCoeff(FuncSpaceData data)
      : _data(data), _basis(BasisFactory::getBezierBasis(data))
  {
    this->resize(getNumCoeff(), 1);
  };
  bezierCoeff(FuncSpaceData data, fullMatrix<double> &lagCoeff)
      : _data(data), _basis(BasisFactory::getBezierBasis(data))
  {
    this->resize(getNumCoeff(), lagCoeff.size2());
    _basis->matrixLag2Bez.mult(lagCoeff, *this);
  };
  bezierCoeff(FuncSpaceData data, fullVector<double> &lagCoeff)
      : _data(data), _basis(BasisFactory::getBezierBasis(data))
  {
    this->resize(getNumCoeff(), 1);
    fullMatrix prox;
    prox.setAsProxy(lagCoeff.getDataPtr(), lagCoeff.size(), 1);
    _basis->matrixLag2Bez.mult(prox, *this);
  };

  inline int getNumCoeff() {return _basis->getNumCoeff();}
  inline int getNumLagCoeff() {return _basis->getNumLagCoeff();}

  void subdivide(std::vector<bezierCoeff> &subCoeff);

private:
  static void _subdivide(fullMatrix<double> &coeff, int n, int start);
  static void _subdivide(fullMatrix<double> &coeff, int n, int start, int inc);
};

#endif
