// Copyright (C) 2013 ULg-UCL
//
// Permission is hereby granted, free of charge, to any person
// obtaining a copy of this software and associated documentation
// files (the "Software"), to deal in the Software without
// restriction, including without limitation the rights to use, copy,
// modify, merge, publish, distribute, and/or sell copies of the
// Software, and to permit persons to whom the Software is furnished
// to do so, provided that the above copyright notice(s) and this
// permission notice appear in all copies of the Software and that
// both the above copyright notice(s) and this permission notice
// appear in supporting documentation.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT OF THIRD PARTY RIGHTS. IN NO EVENT SHALL THE
// COPYRIGHT HOLDER OR HOLDERS INCLUDED IN THIS NOTICE BE LIABLE FOR
// ANY CLAIM, OR ANY SPECIAL INDIRECT OR CONSEQUENTIAL DAMAGES, OR ANY
// DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
// WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
// ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
// OF THIS SOFTWARE.
//
// Please report all bugs and problems to the public mailing list
// <gmsh@geuz.org>.
//
// Contributors: Thomas Toulorge, Jonathan Lambrechts

#ifndef _MESHOPTPATCH_H_
#define _MESHOPTPATCH_H_

#include <vector>
#include <map>
#include <set>
#include "fullMatrix.h"
#include "SPoint3.h"
#include "MeshOptVertexCoord.h"


class GEntity;
class MVertex;
class MElement;


class Patch
{
public:
  Patch(const std::map<MElement*,GEntity*> &element2entity,
        const std::set<MElement*> &els, std::set<MVertex*> &toFix, bool fixBndNodes);

  // Mesh entities and variables
  inline const int &dim() { return _dim; }
  inline const int &nPC() { return _nPC; }
  inline int nVert() { return _vert.size(); }
  inline int nFV() { return _freeVert.size(); }
  inline const bool forced(int iV) { return _forced[iV]; }
  inline int nEl() { return _el.size(); }
  inline const int &nPCFV(int iFV) { return _nPCFV[iFV]; }
  inline int indPCFV(int iFV, int iPC) { return _startPCFV[iFV]+iPC; }
  inline int nPCEl(int iEl) { return _indPCEl[iEl].size(); }
  inline const int &nNodEl(int iEl) { return _nNodEl[iEl]; }
  inline const int &indPCEl(int iEl, int iPC) { return _indPCEl[iEl][iPC]; }
  inline const int &el2V(int iEl, int i) { return _el2V[iEl][i]; }
  inline const int &el2FV(int iEl, int i) { return _el2FV[iEl][i]; }
  inline MElement *el(int iEl) { return _el[iEl]; }
  inline const int &fv2V(int iFV) { return _fv2V[iFV]; }
  inline const SPoint3 &xyz(int iV) { return _xyz[iV]; }
  inline const SPoint3 &ixyz(int iV) { return _ixyz[iV]; }
  inline const SPoint3 &uvwFV(int iFV) { return _uvw[iFV]; }
  inline void gXyz2gUvw(int iFV, const SPoint3 &gXyz, SPoint3 &gUvw);
  inline void gXyz2gUvw(int iFV, const std::vector<SPoint3> &gXyz,
                                        std::vector<SPoint3> &gUvw);
  void pcScale(int iFV, std::vector<double> &scale);                                      // Calc. scale of param. coord. for precond.
  void getUvw(double *it);
  void updateMesh(const double *it);
  void updateGEntityPositions();
  void writeMSH(const char *filename);

  // Node distance measure
  void initScaledNodeDispSq();
  inline double invLengthScaleSq() { return _invLengthScaleSq; }
  double scaledNodeDispSq(int iFV);
  void gradScaledNodeDispSq(int iFV, std::vector<double> &gDSq);

  // High-order: scaled Jacobian and metric measures, distance to CAD
  inline const int &nBezEl(int iEl) { return _nBezEl[iEl]; }
  inline int indGSJ(int iEl, int l, int iPC) { return iPC*_nBezEl[iEl]+l; }
  void initScaledJac();
  void scaledJacAndGradients(int iEl, std::vector<double> &sJ, std::vector<double> &gSJ);
  void initMetricMin();
  void metricMinAndGradients(int iEl, std::vector<double> &sJ, std::vector<double> &gSJ);
  bool bndDistAndGradients(int iEl, double &f , std::vector<double> &gradF, double eps);

  // Mesh quality
  inline const int &nNCJEl(int iEl) { return _nNCJEl[iEl]; }
  inline int indGNCJ(int iEl, int l, int iPC) { return iPC*_nNCJEl[iEl]+l; }
  void initNCJ();
  void NCJ(int iEl, std::vector<double> &NCJ);
  void NCJAndGradients(int iEl, std::vector<double> &NCJ, std::vector<double> &gNCJ);

private:

  // Mesh entities and variables
  int _dim;
  int _nPC;                                         // Total nb. of parametric coordinates
  std::vector<MElement*> _el;                       // List of elements
  std::vector<MVertex*> _vert, _freeVert;           // List of vert., free vert.
  std::vector<int> _fv2V;                           // Index of free vert. -> index of vert.
  std::vector<bool> _forced;                        // Is vertex forced?
  std::vector<SPoint3> _xyz, _ixyz;                 // Physical coord. of ALL vertices (current, straight, init.)
  std::vector<SPoint3> _uvw, _iuvw;                 // Parametric coord. of FREE vertices (current, straight, init.)
  std::vector<int> _startPCFV;                      // Start index of parametric coordinates for a free vertex
  std::vector<int> _nPCFV;                          // Number of parametric coordinates for a free vertex
  std::vector<std::vector<int> > _el2FV, _el2V;     // Free vertices, vertices in element
  std::vector<int> _nNodEl;                         // Number of Bezier poly. and nodes for an el.
  std::vector<std::vector<int> > _indPCEl;          // Index of parametric coord. for an el.
  std::vector<VertexCoord*> _coordFV;               // Parametrization for a free vertex
  int addVert(MVertex* vert);
  int addFreeVert(MVertex* vert, const int iV, const int nPCV,
                  VertexCoord *param, std::set<MVertex*> &toFix);
  static inline int indJB2DBase(int nNod, int l, int i, int j)
  {
    return (l*nNod+i)*nNod+j;
  }
  inline int indJB2D(int iEl, int l, int i, int j)
  {
    return indJB2DBase(_nNodEl[iEl],l,i,j);
  }
  static inline int indJB3DBase(int nNod, int l, int i, int j, int m)
  {
    return ((l*nNod+i)*nNod+j)*nNod+m;
  }
  inline int indJB3D(int iEl, int l, int i, int j, int m)
  {
    return indJB3DBase(_nNodEl[iEl],l,i,j,m);
  }

  // Node displacement
  double _invLengthScaleSq;                             // Square inverse of a length for node displacement scaling

  // High-order: scaled Jacobian and metric measures
  std::vector<int> _nBezEl;                             // Number of Bezier poly. for an el.
  std::vector<fullMatrix<double> > _scaledNormEl;       // Normals to 2D elements for Jacobian regularization and scaling
  std::vector<double> _invStraightJac;                  // Initial Jacobians for 3D elements
//  void calcScaledNormalEl2D(const std::map<MElement*,GEntity*> &element2entity, int iEl);
  void calcScaledNormalEl2D(int iEl);

  // Mesh quality
  std::vector<int> _nNCJEl;                             // Number of NCJ values for an el.
};


inline void Patch::gXyz2gUvw(int iFV, const SPoint3 &gXyz, SPoint3 &gUvw)
{
  _coordFV[iFV]->gXyz2gUvw(_uvw[iFV], gXyz, gUvw);
}


inline void Patch::gXyz2gUvw(int iFV, const std::vector<SPoint3> &gXyz,
                                            std::vector<SPoint3> &gUvw)
{
  _coordFV[iFV]->gXyz2gUvw(_uvw[iFV], gXyz, gUvw);
}


#endif