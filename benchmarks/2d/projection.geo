//
Point(1) = {0,0,0,1};
Point(2) = {0,10,0,1};
Line(1) = {2,1};
Extrude Line {1, {50.0,0.0,0.0}, {1,0,0}, {0,5,0}, 2*Pi}{Recombine;Layers{50,9000,1};};
//
Point(1000) = {0,0,20,1};
Point(2000) = {0,10,20,1};
Line(1000) = {2000,1000};
Extrude Line {1000, {50.0,0.0,0.0}, {1,0,0}, {0,5,20}, 2*Pi};



