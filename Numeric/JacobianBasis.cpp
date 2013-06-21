// Gmsh - Copyright (C) 1997-2013 C. Geuzaine, J.-F. Remacle
//
// See the LICENSE.txt file for license information. Please report all
// bugs and problems to the public mailing list <gmsh@geuz.org>.

#include "GmshDefines.h"
#include "GmshMessage.h"

#include <vector>
#include "polynomialBasis.h"
#include "pyramidalBasis.h"
#include "pointsGenerators.h"
#include "BasisFactory.h"
#include "Numeric.h"


JacobianBasis::JacobianBasis(int tag)
{
  const int parentType = ElementType::ParentTypeFromTag(tag);
  const int order = ElementType::OrderFromTag(tag);

  int jacobianOrder, primJacobianOrder;
  switch (parentType) {
    case TYPE_PNT :
      primJacobianOrder = 0;
      jacobianOrder = 0;
      lagPoints = gmshGeneratePointsLine(0);
      break;
    case TYPE_LIN :
      primJacobianOrder = 0;
      jacobianOrder = order - 1;
      lagPoints = gmshGeneratePointsLine(jacobianOrder);
      break;
    case TYPE_TRI :
      primJacobianOrder = 0;
      jacobianOrder = 2*order - 2;
      lagPoints = gmshGeneratePointsTriangle(jacobianOrder);
      break;
    case TYPE_QUA :
      primJacobianOrder = 1;
      jacobianOrder = 2*order - 1;
      lagPoints = gmshGeneratePointsQuadrangle(jacobianOrder);
      break;
    case TYPE_TET :
      primJacobianOrder = 0;
      jacobianOrder = 3*order - 3;
      lagPoints = gmshGeneratePointsTetrahedron(jacobianOrder);
      break;
    case TYPE_PRI :
      primJacobianOrder = 2;
      jacobianOrder = 3*order - 1;
      lagPoints = gmshGeneratePointsPrism(jacobianOrder);
      break;
    case TYPE_HEX :
      primJacobianOrder = 2;
      jacobianOrder = 3*order - 1;
      lagPoints = gmshGeneratePointsHexahedron(jacobianOrder);
      break;
    case TYPE_PYR :
      primJacobianOrder = 0;
      jacobianOrder = 3*order - 3;
      lagPoints = generateJacPointsPyramid(jacobianOrder);
      break;
    default :
      Msg::Error("Unknown Jacobian function space for element type %d", tag);
      jacobianOrder = 0;
      break;
  }
  numJacNodes = lagPoints.size1();

  // Store Bezier basis
  bezier = BasisFactory::getBezierBasis(parentType, jacobianOrder);

  // Store shape function gradients of mapping at Jacobian nodes
  fullMatrix<double> allDPsi;
  const nodalBasis *mapBasis = BasisFactory::getNodalBasis(tag);

  mapBasis->df(lagPoints, allDPsi);
  numMapNodes = allDPsi.size1();

  gradShapeMatX.resize(numJacNodes, numMapNodes);
  gradShapeMatY.resize(numJacNodes, numMapNodes);
  gradShapeMatZ.resize(numJacNodes, numMapNodes);
  for (int i=0; i<numJacNodes; i++) {
    for (int j=0; j<numMapNodes; j++) {
      gradShapeMatX(i, j) = allDPsi(j, 3*i);
      gradShapeMatY(i, j) = allDPsi(j, 3*i+1);
      gradShapeMatZ(i, j) = allDPsi(j, 3*i+2);
    }
  }

  // Compute matrix for lifting from primary Jacobian basis to Jacobian basis
  int primJacType = ElementType::getTag(parentType, primJacobianOrder, false);
  const nodalBasis *primJacBasis = BasisFactory::getNodalBasis(primJacType);
  numPrimJacNodes = primJacBasis->getNumShapeFunctions();

  matrixPrimJac2Jac.resize(numJacNodes,numPrimJacNodes);
  primJacBasis->f(lagPoints, matrixPrimJac2Jac);

  // Compute shape function gradients of primary mapping at barycenter,
  // in order to compute normal to straight element
  const int primMapType = ElementType::getTag(parentType, 1, false);
  const nodalBasis *primMapBasis = BasisFactory::getNodalBasis(primMapType);
  numPrimMapNodes = primMapBasis->getNumShapeFunctions();

  double xBar = 0., yBar = 0., zBar = 0.;
  double barycenter[3] = {0., 0., 0.};
  for (int i = 0; i < numPrimMapNodes; i++) {
    for (int j = 0; i < primMapBasis->points.size2(); ++i) {
      barycenter[j] += primMapBasis->points(i, j);
    }
  }
  barycenter[0] /= numPrimMapNodes;
  barycenter[1] /= numPrimMapNodes;
  barycenter[2] /= numPrimMapNodes;

  double (*barDPsi)[3] = new double[numPrimMapNodes][3];
  primMapBasis->df(xBar, yBar, zBar, barDPsi);

  primGradShapeBarycenterX.resize(numPrimMapNodes);
  primGradShapeBarycenterY.resize(numPrimMapNodes);
  primGradShapeBarycenterZ.resize(numPrimMapNodes);
  for (int j=0; j<numPrimMapNodes; j++) {
    primGradShapeBarycenterX(j) = barDPsi[j][0];
    primGradShapeBarycenterY(j) = barDPsi[j][1];
    primGradShapeBarycenterZ(j) = barDPsi[j][2];
  }
  delete[] barDPsi;
}

// Computes (unit) normals to straight line element at barycenter (with norm of gradient as return value)
double JacobianBasis::getPrimNormals1D(const fullMatrix<double> &nodesXYZ, fullMatrix<double> &result) const
{

  fullVector<double> dxyzdXbar(3);
  for (int j=0; j<numPrimMapNodes; j++) {
    dxyzdXbar(0) += primGradShapeBarycenterX(j)*nodesXYZ(j,0);
    dxyzdXbar(1) += primGradShapeBarycenterX(j)*nodesXYZ(j,1);
    dxyzdXbar(2) += primGradShapeBarycenterX(j)*nodesXYZ(j,2);
  }

  if((fabs(dxyzdXbar(0)) >= fabs(dxyzdXbar(1)) && fabs(dxyzdXbar(0)) >= fabs(dxyzdXbar(2))) ||
     (fabs(dxyzdXbar(1)) >= fabs(dxyzdXbar(0)) && fabs(dxyzdXbar(1)) >= fabs(dxyzdXbar(2)))) {
    result(0,0) = dxyzdXbar(1); result(0,1) = -dxyzdXbar(0); result(0,2) = 0.;
  }
  else {
    result(0,0) = 0.; result(0,1) = dxyzdXbar(2); result(0,2) = -dxyzdXbar(1);
  }
  const double norm0 = sqrt(result(0,0)*result(0,0)+result(0,1)*result(0,1)+result(0,2)*result(0,2));
  result(0,0) /= norm0; result(0,1) /= norm0; result(0,2) /= norm0;

  result(1,2) = dxyzdXbar(0) * result(0,1) - dxyzdXbar(1) * result(0,0);
  result(1,1) = -dxyzdXbar(0) * result(0,2) + dxyzdXbar(2) * result(0,0);
  result(1,0) = dxyzdXbar(1) * result(0,2) - dxyzdXbar(2) * result(0,1);
  const double norm1 = sqrt(result(1,0)*result(1,0)+result(1,1)*result(1,1)+result(1,2)*result(1,2));
  result(1,0) /= norm1; result(1,1) /= norm1; result(1,2) /= norm1;

  return sqrt(dxyzdXbar(0)*dxyzdXbar(0)+dxyzdXbar(1)*dxyzdXbar(1)+dxyzdXbar(2)*dxyzdXbar(2));

}

// Computes (unit) normal to straight surface element at barycenter (with norm as return value)
double JacobianBasis::getPrimNormal2D(const fullMatrix<double> &nodesXYZ, fullMatrix<double> &result) const
{

  fullVector<double> dxyzdXbar(3), dxyzdYbar(3);
  for (int j=0; j<numPrimMapNodes; j++) {
    dxyzdXbar(0) += primGradShapeBarycenterX(j)*nodesXYZ(j,0);
    dxyzdXbar(1) += primGradShapeBarycenterX(j)*nodesXYZ(j,1);
    dxyzdXbar(2) += primGradShapeBarycenterX(j)*nodesXYZ(j,2);
    dxyzdYbar(0) += primGradShapeBarycenterY(j)*nodesXYZ(j,0);
    dxyzdYbar(1) += primGradShapeBarycenterY(j)*nodesXYZ(j,1);
    dxyzdYbar(2) += primGradShapeBarycenterY(j)*nodesXYZ(j,2);
  }

  result(0,2) = dxyzdXbar(0) * dxyzdYbar(1) - dxyzdXbar(1) * dxyzdYbar(0);
  result(0,1) = -dxyzdXbar(0) * dxyzdYbar(2) + dxyzdXbar(2) * dxyzdYbar(0);
  result(0,0) = dxyzdXbar(1) * dxyzdYbar(2) - dxyzdXbar(2) * dxyzdYbar(1);
  const double norm0 = sqrt(result(0,0)*result(0,0)+result(0,1)*result(0,1)+result(0,2)*result(0,2));
  result(0,0) /= norm0; result(0,1) /= norm0; result(0,2) /= norm0;

  return norm0;

}

static inline double calcDet3D(double dxdX, double dydX, double dzdX,
                               double dxdY, double dydY, double dzdY,
                               double dxdZ, double dydZ, double dzdZ)
{
  return dxdX*dydY*dzdZ + dxdY*dydZ*dzdX + dydX*dzdY*dxdZ
       - dxdZ*dydY*dzdX - dxdY*dydX*dzdZ - dydZ*dzdY*dxdX;
}

// Returns absolute value of Jacobian of straight volume element at barycenter
double JacobianBasis::getPrimJac3D(const fullMatrix<double> &nodesXYZ) const
{

  double dxdX = 0, dydX = 0, dzdX = 0, dxdY = 0, dydY = 0, dzdY = 0, dxdZ = 0, dydZ = 0, dzdZ = 0;
  for (int j=0; j<numPrimMapNodes; j++) {
    dxdX += primGradShapeBarycenterX(j)*nodesXYZ(j,0);
    dydX += primGradShapeBarycenterX(j)*nodesXYZ(j,1);
    dzdX += primGradShapeBarycenterX(j)*nodesXYZ(j,2);
    dxdY += primGradShapeBarycenterY(j)*nodesXYZ(j,0);
    dydY += primGradShapeBarycenterY(j)*nodesXYZ(j,1);
    dzdY += primGradShapeBarycenterY(j)*nodesXYZ(j,2);
    dxdZ += primGradShapeBarycenterZ(j)*nodesXYZ(j,0);
    dydZ += primGradShapeBarycenterZ(j)*nodesXYZ(j,1);
    dzdZ += primGradShapeBarycenterZ(j)*nodesXYZ(j,2);
  }

  return fabs(calcDet3D(dxdX,dydX,dzdX,dxdY,dydY,dzdY,dxdZ,dydZ,dzdZ));

}

// Calculate (signed) Jacobian at mapping's nodes for one element, with normal vectors to
// straight element for regularization
void JacobianBasis::getSignedJacobian(const fullMatrix<double> &nodesXYZ, fullVector<double> &jacobian) const
{

  switch (bezier->getDim()) {

    case 1 : {
      fullMatrix<double> normals(2,3);
      getPrimNormals1D(nodesXYZ,normals);
      getSignedJacobian(nodesXYZ,normals,jacobian);
      break;
    }

    case 2 : {
      fullMatrix<double> normal(1,3);
      getPrimNormal2D(nodesXYZ,normal);
      getSignedJacobian(nodesXYZ,normal,jacobian);
      break;
    }

    case 0 :
    case 3 : {
      fullMatrix<double> dum;
      getSignedJacobian(nodesXYZ,dum,jacobian);
      break;
    }

  }

}

// Calculate scaled (signed) Jacobian at mapping's nodes for one element, with normal vectors to
// straight element for regularization and scaling
void JacobianBasis::getScaledJacobian(const fullMatrix<double> &nodesXYZ, fullVector<double> &jacobian) const
{

  switch (bezier->getDim()) {

    case 1 : {
      fullMatrix<double> normals(2,3);
      const double scale = 1./getPrimNormals1D(nodesXYZ,normals);
      normals(0,0) *= scale; normals(0,1) *= scale; normals(0,2) *= scale;  // Faster to scale 1 normal than afterwards
      getSignedJacobian(nodesXYZ,normals,jacobian);
      break;
    }

    case 2 : {
      fullMatrix<double> normal(1,3);
      const double scale = 1./getPrimNormal2D(nodesXYZ,normal);
      normal(0,0) *= scale; normal(0,1) *= scale; normal(0,2) *= scale;     // Faster to scale normal than afterwards
      getSignedJacobian(nodesXYZ,normal,jacobian);
      break;
    }

    case 0 :
    case 3 : {
      fullMatrix<double> dum;
      const double scale = 1./getPrimJac3D(nodesXYZ);
      getSignedJacobian(nodesXYZ,dum,jacobian);
      jacobian.scale(scale);
      break;
    }

  }

}

// Calculate (signed) Jacobian at mapping's nodes for one element, given vectors for regularization
// of line and surface Jacobians in 3D
void JacobianBasis::getSignedJacobian(const fullMatrix<double> &nodesXYZ,
                                      const fullMatrix<double> &normals, fullVector<double> &jacobian) const
{

  switch (bezier->getDim()) {

    case 0 : {
      for (int i = 0; i < numJacNodes; i++) jacobian(i) = 1.;
      break;
    }

    case 1 : {
      fullMatrix<double> dxyzdX(numJacNodes,3);
      gradShapeMatX.mult(nodesXYZ, dxyzdX);
      for (int i = 0; i < numJacNodes; i++) {
        const double &dxdX = dxyzdX(i,0), &dydX = dxyzdX(i,1), &dzdX = dxyzdX(i,2);
        const double &dxdY = normals(0,0), &dydY = normals(0,1), &dzdY = normals(0,2);
        const double &dxdZ = normals(1,0), &dydZ = normals(1,1), &dzdZ = normals(1,2);
        jacobian(i) = calcDet3D(dxdX,dydX,dzdX,dxdY,dydY,dzdY,dxdZ,dydZ,dzdZ);
      }
      break;
    }

    case 2 : {
      fullMatrix<double> dxyzdX(numJacNodes,3), dxyzdY(numJacNodes,3);
      gradShapeMatX.mult(nodesXYZ, dxyzdX);
      gradShapeMatY.mult(nodesXYZ, dxyzdY);
      for (int i = 0; i < numJacNodes; i++) {
        const double &dxdX = dxyzdX(i,0), &dydX = dxyzdX(i,1), &dzdX = dxyzdX(i,2);
        const double &dxdY = dxyzdY(i,0), &dydY = dxyzdY(i,1), &dzdY = dxyzdY(i,2);
        const double &dxdZ = normals(0,0), &dydZ = normals(0,1), &dzdZ = normals(0,2);
        jacobian(i) = calcDet3D(dxdX,dydX,dzdX,dxdY,dydY,dzdY,dxdZ,dydZ,dzdZ);
      }
      break;
    }

    case 3 : {
      fullMatrix<double> dxyzdX(numJacNodes,3), dxyzdY(numJacNodes,3), dxyzdZ(numJacNodes,3);
      gradShapeMatX.mult(nodesXYZ, dxyzdX);
      gradShapeMatY.mult(nodesXYZ, dxyzdY);
      gradShapeMatZ.mult(nodesXYZ, dxyzdZ);
      for (int i = 0; i < numJacNodes; i++) {
        const double &dxdX = dxyzdX(i,0), &dydX = dxyzdX(i,1), &dzdX = dxyzdX(i,2);
        const double &dxdY = dxyzdY(i,0), &dydY = dxyzdY(i,1), &dzdY = dxyzdY(i,2);
        const double &dxdZ = dxyzdZ(i,0), &dydZ = dxyzdZ(i,1), &dzdZ = dxyzdZ(i,2);
        jacobian(i) = calcDet3D(dxdX,dydX,dzdX,dxdY,dydY,dzdY,dxdZ,dydZ,dzdZ);
      }
      break;
    }

  }

}

// Calculate (signed) Jacobian at mapping's nodes for one element, given vectors for regularization
// of line and surface Jacobians in 3D
void JacobianBasis::getSignedJacAndGradients(const fullMatrix<double> &nodesXYZ,
                                             const fullMatrix<double> &normals,
                                             fullMatrix<double> &JDJ) const
{

  switch (bezier->getDim()) {

    case 0 : {
      for (int i = 0; i < numJacNodes; i++) {
        for (int j = 0; j < numMapNodes; j++) {
          JDJ (i,j) = 0.;
          JDJ (i,j+1*numMapNodes) = 0.;
          JDJ (i,j+2*numMapNodes) = 0.;
        }
        JDJ(i,3*numMapNodes) = 1.;
      }
      break;
    }

    case 1 : {
      fullMatrix<double> dxyzdX(numJacNodes,3), dxyzdY(numJacNodes,3);
      gradShapeMatX.mult(nodesXYZ, dxyzdX);
      for (int i = 0; i < numJacNodes; i++) {
        const double &dxdX = dxyzdX(i,0), &dydX = dxyzdX(i,1), &dzdX = dxyzdX(i,2);
        const double &dxdY = normals(0,0), &dydY = normals(0,1), &dzdY = normals(0,2);
        const double &dxdZ = normals(1,0), &dydZ = normals(1,1), &dzdZ = normals(1,2);
        for (int j = 0; j < numMapNodes; j++) {
          const double &dPhidX = gradShapeMatX(i,j);
          JDJ (i,j) = dPhidX * dydY * dzdZ + dPhidX * dzdY * dydZ;
          JDJ (i,j+1*numMapNodes) = dPhidX * dzdY * dxdZ - dPhidX * dxdY * dzdZ;
          JDJ (i,j+2*numMapNodes) = dPhidX * dxdY * dydZ - dPhidX * dydY * dxdZ;
        }
        JDJ(i,3*numMapNodes) = calcDet3D(dxdX,dydX,dzdX,dxdY,dydY,dzdY,dxdZ,dydZ,dzdZ);
      }
      break;
    }

    case 2 : {
      fullMatrix<double> dxyzdX(numJacNodes,3), dxyzdY(numJacNodes,3);
      gradShapeMatX.mult(nodesXYZ, dxyzdX);
      gradShapeMatY.mult(nodesXYZ, dxyzdY);
      for (int i = 0; i < numJacNodes; i++) {
        const double &dxdX = dxyzdX(i,0), &dydX = dxyzdX(i,1), &dzdX = dxyzdX(i,2);
        const double &dxdY = dxyzdY(i,0), &dydY = dxyzdY(i,1), &dzdY = dxyzdY(i,2);
        const double &dxdZ = normals(0,0), &dydZ = normals(0,1), &dzdZ = normals(0,2);
        for (int j = 0; j < numMapNodes; j++) {
          const double &dPhidX = gradShapeMatX(i,j);
          const double &dPhidY = gradShapeMatY(i,j);
          JDJ (i,j) =
            dPhidX * dydY * dzdZ + dzdX * dPhidY * dydZ +
            dPhidX * dzdY * dydZ - dydX * dPhidY * dzdZ;
          JDJ (i,j+1*numMapNodes) =
            dxdX * dPhidY * dzdZ +
            dPhidX * dzdY * dxdZ - dzdX * dPhidY * dxdZ
                                 - dPhidX * dxdY * dzdZ;
          JDJ (i,j+2*numMapNodes) =
                                   dPhidX * dxdY * dydZ +
            dydX * dPhidY * dxdZ - dPhidX * dydY * dxdZ -
            dxdX * dPhidY * dydZ;
        }
        JDJ(i,3*numMapNodes) = calcDet3D(dxdX,dydX,dzdX,dxdY,dydY,dzdY,dxdZ,dydZ,dzdZ);
      }
      break;
    }

    case 3 : {
      fullMatrix<double> dxyzdX(numJacNodes,3), dxyzdY(numJacNodes,3), dxyzdZ(numJacNodes,3);
      gradShapeMatX.mult(nodesXYZ, dxyzdX);
      gradShapeMatY.mult(nodesXYZ, dxyzdY);
      gradShapeMatZ.mult(nodesXYZ, dxyzdZ);
      for (int i = 0; i < numJacNodes; i++) {
        const double &dxdX = dxyzdX(i,0), &dydX = dxyzdX(i,1), &dzdX = dxyzdX(i,2);
        const double &dxdY = dxyzdY(i,0), &dydY = dxyzdY(i,1), &dzdY = dxyzdY(i,2);
        const double &dxdZ = dxyzdZ(i,0), &dydZ = dxyzdZ(i,1), &dzdZ = dxyzdZ(i,2);
        for (int j = 0; j < numMapNodes; j++) {
          const double &dPhidX = gradShapeMatX(i,j);
          const double &dPhidY = gradShapeMatY(i,j);
          const double &dPhidZ = gradShapeMatZ(i,j);
          JDJ (i,j) =
            dPhidX * dydY * dzdZ + dzdX * dPhidY * dydZ +
            dydX * dzdY * dPhidZ - dzdX * dydY * dPhidZ -
            dPhidX * dzdY * dydZ - dydX * dPhidY * dzdZ;
          JDJ (i,j+1*numMapNodes) =
            dxdX * dPhidY * dzdZ + dzdX * dxdY * dPhidZ +
            dPhidX * dzdY * dxdZ - dzdX * dPhidY * dxdZ -
            dxdX * dzdY * dPhidZ - dPhidX * dxdY * dzdZ;
          JDJ (i,j+2*numMapNodes) =
            dxdX * dydY * dPhidZ + dPhidX * dxdY * dydZ +
            dydX * dPhidY * dxdZ - dPhidX * dydY * dxdZ -
            dxdX * dPhidY * dydZ - dydX * dxdY * dPhidZ;
        }
        JDJ(i,3*numMapNodes) = calcDet3D(dxdX,dydX,dzdX,dxdY,dydY,dzdY,dxdZ,dydZ,dzdZ);
      }
      break;
    }

  }

}

void JacobianBasis::getMetricMinAndGradients(const fullMatrix<double> &nodesXYZ,
                                             const fullMatrix<double> &nodesXYZStraight,
                                             fullVector<double> &lambdaJ , fullMatrix<double> &gradLambdaJ) const
{

  // jacobian of the straight elements (only triangles for now)
  SPoint3 v0(nodesXYZ(0,0),nodesXYZ(0,1),nodesXYZ(0,2));
  SPoint3 v1(nodesXYZ(1,0),nodesXYZ(1,1),nodesXYZ(1,2));
  SPoint3 v2(nodesXYZ(2,0),nodesXYZ(2,1),nodesXYZ(2,2));
  SPoint3 *IXYZ[3] = {&v0, &v1, &v2};
  double jaci[2][2] = {
    {IXYZ[1]->x() - IXYZ[0]->x(), IXYZ[2]->x() - IXYZ[0]->x()},
    {IXYZ[1]->y() - IXYZ[0]->y(), IXYZ[2]->y() - IXYZ[0]->y()}
  };
  double invJaci[2][2];
  inv2x2(jaci, invJaci);

  for (int l = 0; l < numJacNodes; l++) {
    double jac[2][2] = {{0., 0.}, {0., 0.}};
    for (int i = 0; i < numMapNodes; i++) {
      const double &dPhidX = gradShapeMatX(l,i);
      const double &dPhidY = gradShapeMatY(l,i);
      const double dpsidx = dPhidX * invJaci[0][0] + dPhidY * invJaci[1][0];
      const double dpsidy = dPhidX * invJaci[0][1] + dPhidY * invJaci[1][1];
      jac[0][0] += nodesXYZ(i,0) * dpsidx;
      jac[0][1] += nodesXYZ(i,0) * dpsidy;
      jac[1][0] += nodesXYZ(i,1) * dpsidx;
      jac[1][1] += nodesXYZ(i,1) * dpsidy;
    }
    const double dxdx = jac[0][0] * jac[0][0] + jac[0][1] * jac[0][1];
    const double dydy = jac[1][0] * jac[1][0] + jac[1][1] * jac[1][1];
    const double dxdy = jac[0][0] * jac[1][0] + jac[0][1] * jac[1][1];
    const double sqr = sqrt((dxdx - dydy) * (dxdx - dydy) + 4 * dxdy * dxdy);
    const double osqr = sqr > 1e-8 ? 1/sqr : 0;
    lambdaJ(l) = 0.5 * (dxdx + dydy - sqr);
    const double axx = (1 - (dxdx - dydy) * osqr) * jac[0][0] - 2 * dxdy * osqr * jac[1][0];
    const double axy = (1 - (dxdx - dydy) * osqr) * jac[0][1] - 2 * dxdy * osqr * jac[1][1];
    const double ayx = -2 * dxdy * osqr * jac[0][0] + (1 - (dydy - dxdx) * osqr) * jac[1][0];
    const double ayy = -2 * dxdy * osqr * jac[0][1] + (1 - (dydy - dxdx) * osqr) * jac[1][1];
    const double axixi   = axx * invJaci[0][0] + axy * invJaci[0][1];
    const double aetaeta = ayx * invJaci[1][0] + ayy * invJaci[1][1];
    const double aetaxi  = ayx * invJaci[0][0] + ayy * invJaci[0][1];
    const double axieta  = axx * invJaci[1][0] + axy * invJaci[1][1];
    for (int i = 0; i < numMapNodes; i++) {
      const double &dPhidX = gradShapeMatX(l,i);
      const double &dPhidY = gradShapeMatY(l,i);
      gradLambdaJ(l, i + 0 * numMapNodes) = axixi * dPhidX + axieta * dPhidY;
      gradLambdaJ(l, i + 1 * numMapNodes) = aetaxi * dPhidX + aetaeta * dPhidY;
    }
  }

}

// Calculate (signed) Jacobian at mapping's nodes for several elements
// TODO: Optimize and test 1D & 2D
void JacobianBasis::getSignedJacobian(const fullMatrix<double> &nodesX, const fullMatrix<double> &nodesY,
                                      const fullMatrix<double> &nodesZ, fullMatrix<double> &jacobian) const
{

  switch (bezier->getDim()) {

    case 0 : {
      const int numEl = nodesX.size2();
      for (int iEl = 0; iEl < numEl; iEl++)
        for (int i = 0; i < numJacNodes; i++) jacobian(i,iEl) = 1.;
      break;
    }

    case 1 : {
      const int numEl = nodesX.size2();
      fullMatrix<double> dxdX(numJacNodes,numEl), dydX(numJacNodes,numEl), dzdX(numJacNodes,numEl);
      gradShapeMatX.mult(nodesX, dxdX); gradShapeMatX.mult(nodesY, dydX); gradShapeMatX.mult(nodesZ, dzdX);
      for (int iEl = 0; iEl < numEl; iEl++) {
        fullMatrix<double> nodesXYZ(numPrimJacNodes,3);
        for (int i = 0; i < numPrimJacNodes; i++) {
          nodesXYZ(i,0) = nodesX(i,iEl);
          nodesXYZ(i,1) = nodesY(i,iEl);
          nodesXYZ(i,2) = nodesZ(i,iEl);
        }
        fullMatrix<double> normals(2,3);
        getPrimNormals1D(nodesXYZ,normals);
        const double &dxdY = normals(0,0), &dydY = normals(0,1), &dzdY = normals(0,2);
        const double &dxdZ = normals(1,0), &dydZ = normals(1,1), &dzdZ = normals(1,2);
        for (int i = 0; i < numJacNodes; i++)
          jacobian(i,iEl) = calcDet3D(dxdX(i,iEl),dydX(i,iEl),dzdX(i,iEl),
                                      dxdY,dydY,dzdY,
                                      dxdZ,dydZ,dzdZ);
      }
      break;
    }

    case 2 : {
      const int numEl = nodesX.size2();
      fullMatrix<double> dxdX(numJacNodes,numEl), dydX(numJacNodes,numEl), dzdX(numJacNodes,numEl);
      fullMatrix<double> dxdY(numJacNodes,numEl), dydY(numJacNodes,numEl), dzdY(numJacNodes,numEl);
      gradShapeMatX.mult(nodesX, dxdX); gradShapeMatX.mult(nodesY, dydX); gradShapeMatX.mult(nodesZ, dzdX);
      gradShapeMatY.mult(nodesX, dxdY); gradShapeMatY.mult(nodesY, dydY); gradShapeMatY.mult(nodesZ, dzdY);
      for (int iEl = 0; iEl < numEl; iEl++) {
        fullMatrix<double> nodesXYZ(numPrimJacNodes,3);
        for (int i = 0; i < numPrimJacNodes; i++) {
          nodesXYZ(i,0) = nodesX(i,iEl);
          nodesXYZ(i,1) = nodesY(i,iEl);
          nodesXYZ(i,2) = nodesZ(i,iEl);
        }
        fullMatrix<double> normal(1,3);
        getPrimNormal2D(nodesXYZ,normal);
        const double &dxdZ = normal(0,0), &dydZ = normal(0,1), &dzdZ = normal(0,2);
        for (int i = 0; i < numJacNodes; i++)
          jacobian(i,iEl) = calcDet3D(dxdX(i,iEl),dydX(i,iEl),dzdX(i,iEl),
                                      dxdY(i,iEl),dydY(i,iEl),dzdY(i,iEl),
                                      dxdZ,dydZ,dzdZ);
      }
      break;
    }

    case 3 : {
      const int numEl = nodesX.size2();
      fullMatrix<double> dxdX(numJacNodes,numEl), dydX(numJacNodes,numEl), dzdX(numJacNodes,numEl);
      fullMatrix<double> dxdY(numJacNodes,numEl), dydY(numJacNodes,numEl), dzdY(numJacNodes,numEl);
      fullMatrix<double> dxdZ(numJacNodes,numEl), dydZ(numJacNodes,numEl), dzdZ(numJacNodes,numEl);
      gradShapeMatX.mult(nodesX, dxdX); gradShapeMatX.mult(nodesY, dydX); gradShapeMatX.mult(nodesZ, dzdX);
      gradShapeMatY.mult(nodesX, dxdY); gradShapeMatY.mult(nodesY, dydY); gradShapeMatY.mult(nodesZ, dzdY);
      gradShapeMatZ.mult(nodesX, dxdZ); gradShapeMatZ.mult(nodesY, dydZ); gradShapeMatZ.mult(nodesZ, dzdZ);
      for (int iEl = 0; iEl < numEl; iEl++)
        for (int i = 0; i < numJacNodes; i++)
          jacobian(i,iEl) = calcDet3D(dxdX(i,iEl),dydX(i,iEl),dzdX(i,iEl),
                                      dxdY(i,iEl),dydY(i,iEl),dzdY(i,iEl),
                                      dxdZ(i,iEl),dydZ(i,iEl),dzdZ(i,iEl));
      break;
    }

  }

}

fullMatrix<double> JacobianBasis::generateJacMonomialsPyramid(int order)
{
  int nbMonomials = (order+3)*((order+3)+1)*(2*(order+3)+1)/6 - 5;
  fullMatrix<double> monomials(nbMonomials, 3);

  if (order == 0) {
    fullMatrix<double> prox, quad = gmshGenerateMonomialsQuadrangle(2);
    prox.setAsProxy(monomials, 0, 2);
    prox.setAll(quad);
    return monomials;
  }

  monomials(0, 0) = 0;
  monomials(0, 1) = 0;
  monomials(0, 2) = 0;

  monomials(1, 0) = order+2;
  monomials(1, 1) = 0;
  monomials(1, 2) = 0;

  monomials(2, 0) = order+2;
  monomials(2, 1) = order+2;
  monomials(2, 2) = 0;

  monomials(3, 0) = 0;
  monomials(3, 1) = order+2;
  monomials(3, 2) = 0;

  monomials(4, 0) = 0;
  monomials(4, 1) = 0;
  monomials(4, 2) = order;

  monomials(5, 0) = 2;
  monomials(5, 1) = 0;
  monomials(5, 2) = order;

  monomials(6, 0) = 2;
  monomials(6, 1) = 2;
  monomials(6, 2) = order;

  monomials(7, 0) = 0;
  monomials(7, 1) = 2;
  monomials(7, 2) = order;

  int index = 8;

  static const int bottom_edges[8][2] = {
    {0, 1},
    {1, 2},
    {2, 3},
    {3, 0}
  };

  // bottom "edges"
  for (int iedge = 0; iedge < 4; ++iedge) {
    int i0 = bottom_edges[iedge][0];
    int i1 = bottom_edges[iedge][1];

    int u_1 = (monomials(i1,0)-monomials(i0,0)) / (order + 2);
    int u_2 = (monomials(i1,1)-monomials(i0,1)) / (order + 2);

    for (int i = 1; i < order + 2; ++i, ++index) {
      monomials(index, 0) = monomials(i0, 0) + i * u_1;
      monomials(index, 1) = monomials(i0, 1) + i * u_2;
      monomials(index, 2) = 0;
    }
  }

  // top "edges"
  for (int iedge = 0; iedge < 4; ++iedge, ++index) {
    int i0 = bottom_edges[iedge][0] + 4;
    int i1 = bottom_edges[iedge][1] + 4;

    int u_1 = (monomials(i1,0)-monomials(i0,0)) / 2;
    int u_2 = (monomials(i1,1)-monomials(i0,1)) / 2;

    monomials(index, 0) = monomials(i0, 0) + u_1;
    monomials(index, 1) = monomials(i0, 1) + u_2;
    monomials(index, 2) = monomials(i0, 2);
  }

  // bottom "face"
  fullMatrix<double> uv = gmshGenerateMonomialsQuadrangle(order);
  uv.add(1);
  for (int i = 0; i < uv.size1(); ++i, ++index) {
    monomials(index, 0) = uv(i, 0);
    monomials(index, 1) = uv(i, 1);
    monomials(index, 2) = 0;
  }

  // top "face"
  monomials(index, 0) = 1;
  monomials(index, 1) = 1;
  monomials(index, 2) = order;
  ++index;

  // other monomials
  for (int k = 1; k < order; ++k) {
    fullMatrix<double> uv = gmshGenerateMonomialsQuadrangle(order + 2 - k);
    for (int i = 0; i < uv.size1(); ++i, ++index) {
      monomials(index, 0) = uv(i, 0);
      monomials(index, 1) = uv(i, 1);
      monomials(index, 2) = k;
    }
  }
  return monomials;
}
