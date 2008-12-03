#include "fourier_syn.h"
#include "../include/algo_helpers.h"
#include <iomanip>

const int NODES = 83;
float iNODES = 1./(float)NODES;
float EPSILON;

float adjust2(float x) {
  float s = 0.8;
  return x+s/(3.*2.*M_PI)*sin(3.*2.*M_PI*x) +
         s/(3.*2.*M_PI)*sin(3.*2.*M_PI*(x+s/(3.*2.*M_PI)*sin(3.*2.*M_PI*x)));
}

float adjust(float x) {
  return (x+0.95/(3.*2.*M_PI)*sin(3.*2.*M_PI*x));
}

float ropelength(FourierKnot &fk);

// XXX this is so slow!!!
void dump(FourierKnot &fk, const char* filename) {
  Curve<Vector3> curve;
  // XXX no node adjustment
  fk.toCurve(NODES,&curve);
  curve.link();
  curve.make_default();
  curve.normalize();
  curve.writePKF(filename);
}

#define randint(from,to) (rand()%(to-from+1)+from)
#define myrand01() ((float)rand()/(float)RAND_MAX)
#define myrand() (2.*(float)rand()/(float)RAND_MAX-1.)
#define randvec() Vector3(myrand(),myrand(),myrand())

void init() {
  srand(time(NULL));
  EPSILON = pow(0.1,randint(3,7));
}

/*
float cut_off(float x,float c):
  if (fabs(x) < c) return 0;
  return x;
}
*/

/*!
  Compute the numerical gradient of the FourierKnot
  knot with epsilon eps. grad is a pointer to a list of
  coefficients.

  Caution : grad is cleared at the beginning of the function!
*/
// XXX not adapted to FourierKnot
void gradient(FourierKnot &knot, Coeffs *grad,
              float eps= 1e-10) {
  Coeff c;
  grad->clear();
  float left, knot_rope = ropelength(knot);
  float iforeps = 1./(4.*eps);
  FourierKnot current(knot);
  for (uint i=0;i<current.csin.size();++i) {
    for (int j=0;j<3;++j) {
      current.csin[i][j] -= eps;
      left = ropelength(current);
      current.csin[i][j] += 2*eps;
      // central difference grad
      c[j] = (2*knot_rope -left - ropelength(current))*iforeps;
      current.csin[i][j] = knot.csin[i][j];
    }
    cout << "c=" << c << endl;
    grad->push_back(c);
  }
  return;
}

// XXX not adapted to FourierKnot
int line_search(FourierKnot *knot, const Coeffs &grad,
                 float delta, float stop) {
  float knot_rope = ropelength(*knot);
  float current_rope = knot_rope + 1;
  float distance;

  FourierKnot current(*knot);

  distance = delta;
  while (distance > stop && current_rope > knot_rope) {
    for (uint i=0;i<current.csin.size();++i) {
      current.csin[i] = knot->csin[i] + distance*grad[i];
    }
    current_rope = ropelength(current);
    cout << "line_search:" << current_rope << endl;
    distance /= 2;
  }
  if (distance > stop || current_rope < knot_rope) {
    *knot = current;
    return 1;
  }
  return 0;
}

// XXX not adapted to FourierKnot
void gradient_flow(FourierKnot *knot) {
  Coeffs grad;
  gradient(*knot,&grad);
  if (line_search(knot, grad, 1e-2, 1e-15))
    cout << "grad -> :)\n";
  else
    cout << "grad -> :(\n";
}

void symmetrize_4_1(FourierKnot *fk) {

   // symmetry group (?) : fm: mirror_y + rot(pi/2)_y   , fr: rot(pi)_y , 
   //                      fmi:  mirror_y + rot(3*pi/2)_y

   cout << "symmetrize_4_1" << endl;
   FourierKnot fr(*fk), fm(*fk), fmi(*fk);

   // First symmetry
   fr.rotate(Vector3(0,1,0),M_PI);
   fr.shift(0.5);
 
   // Second symmetry
   fm.mirror(Vector3(0,1,0));
   fm.rotate(Vector3(0,1,0),M_PI/2);
   fm.shift(-.25);

   // Third symmetry
   fmi.mirror(Vector3(0,1,0));
   fmi.rotate(Vector3(0,1,0),3*M_PI/2);
   fmi.shift(.25);
  
   *fk = (fr+fm+fmi+(*fk))/4.;
}

void zero41coeffs(FourierKnot *fk) {
  // XXX this has to go away !! only for 4.1 
  for (int m=0;m<fk->csin.size();++m) {
    for (int d=0;d<3;++d) {
      for (int i = 0; i<2;++i) {
        if (i==0 && (m+d)%2==1) {
//          cout << knot.csin[m][d] << endl;
          fk->csin[m][d] = 0; 
        }
        if (i==1 && (m+d)%2==1) {
//          cout << knot.ccos[m][d] << endl;
          fk->ccos[m][d] = 0; 
        }
      }
    }
  }
}

#define SIN 0
#define COS 0

const float STEP_CHANGE = 0.05;
float step_min[2], step_max[2];

static void increase(Coeffs &c, int m, int d) {
  c[m][d] *= (1.+STEP_CHANGE);
}

static void decrease(Coeffs &c, int m, int d) {
  c[m][d] *= (1.-STEP_CHANGE);
}

void anneal(float Temp, float Cooling,
            float stop, const char* filename) {

  Coeffs step_size[2];
  Coeff c[2];

  FourierKnot best(filename);
  FourierKnot knot(best);

  float lTemp = Temp;
  float best_rope = ropelength(best);
  float curr_rope = best_rope, knot_rope = best_rope;

  zero41coeffs(&knot);

  step_max[SIN] = step_min[SIN] = fabs(knot.csin[0][0]*.1);
  step_max[COS] = step_min[COS] = fabs(knot.ccos[0][0]*.1);

  for (unsigned int m=0;m<knot.csin.size();++m) {
    for (int i=0;i<2;++i) {
      for (int d=0;d<3;++d) {
        c[i][d] = (i==SIN?fabs(knot.csin[m][d]*.1):fabs(knot.ccos[m][d]*.1));
        if (c[i][d]<1e-20) c[i][d] = 1e-6;

        if (c[i][d]<step_min[i]) step_min[i] = c[i][d];
        else if (c[i][d]>step_max[i]) step_max[i] = c[i][d];
      }
      step_size[i].push_back(c[i]);
    }
  }

  // XXX stop condition
  cout << setprecision(16);
  int m, d, i, steps = 0; float coeff_was;
  while (lTemp > stop) {
    if (steps++ % 100 == 0) {

      step_max[0] = step_max[1] = 0;
      step_min[0] = step_min[1] = 1e20;
      for (unsigned int m=0;m<knot.csin.size();++m) {
        for (int d=0;d<3;++d) {
          for (int i=0;i<2;++i) {
            if (step_size[i][m][d]<step_min[i])
              step_min[i] = step_size[i][m][d];
            else if (step_size[i][m][d]>step_max[i])
              step_max[i] = step_size[i][m][d];
          }
        }
      }

      cout << "E=" << setprecision(8) << knot_rope << "  T=" << setprecision(4)
           << lTemp << setprecision(4)
           << "  s=" << step_min[0] << "/" << step_max[0] << "-"
           << step_min[1] << "/" << step_max[1]
           << "  Diff=" << best_rope - 21.04472593 << endl;
    }

// XXX this has to go away !! only for 4.1 
again:
    m = rand()%knot.csin.size();
    d = rand()%3;
    i = rand()%2;
    if (i==SIN && (m+d) % 2==1) {
      knot.csin[m][d] = 0; 
      goto again; 
    }
    if (i==COS && (m+d) % 2==1) {
      knot.ccos[m][d] = 0; 
      goto again; 
    }
//

    if (i==SIN) {
      coeff_was = knot.csin[m][d];
      knot.csin[m][d] += step_size[SIN][m][d]*myrand();
    }
    else { // COS
      coeff_was = knot.ccos[m][d];
      knot.ccos[m][d] += step_size[COS][m][d]*myrand();
    }
  
    knot_rope = ropelength(knot);
    if (knot_rope < best_rope) {
/*
XXX gradient_flow not ready
      if (myrand01() < 0.01) {
        gradient_flow(&knot);
      }
*/
      knot_rope = ropelength(knot);
      cout << "anneal : " << setprecision(10) << knot_rope << " -> :)\n";
      best_rope = knot_rope;
      curr_rope = knot_rope;
      best = knot;
      ofstream of(filename);
      of << setprecision(16) << best;
      of.close();
      // dump(best,"test.pkf");
      increase(step_size[i],m,d);

      if (myrand01() < 0.1) {
        symmetrize_4_1(&knot);
        zero41coeffs(&knot);
      }

    }
    else if (myrand01() <= exp(-(knot_rope-curr_rope)/lTemp)) {
      curr_rope = knot_rope;
      increase(step_size[i],m,d);
    }
    else {
      if (i==SIN)
        knot.csin[m][d] = coeff_was;
      else
        knot.ccos[m][d] = coeff_was;
      knot_rope = curr_rope;
      decrease(step_size[i],m,d);
    }
    lTemp *= (1.-Cooling);
  }
}

// XXX not adapted to FourierKnot
void cook(FourierKnot &knot, float eps= 0.0001) {

  // ???
  int m = randint(1, knot.csin.size());
  int n; float fac; Vector3 v;
//  ns = random.sample(range(1,len(coeff)),  random.randint(1,m))
//  for n in ns:
  for (int i=0;i<m;++i) {
    n = rand()%knot.csin.size();
    // XXX scale correctly, since csin[n][0,1,2] are different harmonics
    fac = eps/pow(n+1,1.5);
    v = randvec();
    knot.csin[n] += fac*v;
  }
}

/* TODO
def symmetrize(coeff):
   f1f = FourierKnot(coeff[:])
   f1f.rotate([1,0,0],math.pi)
   f1f.flip_dir()

   f2 = FourierKnot(coeff[:])
   f2.shift(1./3.)
   f2.rotate([0,0,1],math.pi* 2./3.)
   f2f = FourierKnot(f2.coeff[:])
   f2f.rotate([-0.5, math.sqrt(3)/2., 0],math.pi)
   f2f.flip_dir(2./3.)

   f3 = FourierKnot(coeff[:])
   f3.shift(2./3.)
   f3.rotate([0,0,1],math.pi* 4./3.)
   f3f = FourierKnot(f3.coeff[:])
   f3f.rotate([-0.5, -math.sqrt(3)/2., 0],math.pi)
   f3f.flip_dir(1./3.)

   for i in xrange(len(coeff)):
     coeff[i] = (coeff[i] + f1f.coeff[i] + f2.coeff[i] + f2f.coeff[i] + f3.coeff[i] + f3f.coeff[i])/6.
*/

float ropelength(FourierKnot &fk) {
  Curve<Vector3> curve;
  fk.toCurve(NODES,&curve);
  curve.link();
  curve.make_default();
  float D = curve.thickness();
  float L = curve.length();
  return L/D;
}

// XXX not adapted to FourierKnot
void improve(const char *filename) {

  const int ITERATIONS = 100;

/*
  coeff =      [vector([0,0,0]),
      vector([0, 0, 0]),vector([0, 0, 0]),     #1
      vector([1,0,0]),vector([0,1,0]),         #2
      vector([0, 0.1, 0]),vector([0, 0, 0.1]),     #3
      vector([0, 0, 0]),vector([0, 0, 0]),     #4
      vector([0.2, 0, 0]),vector([0, 0.2, 0]), #5
      ]
*/

  FourierKnot fk(filename), best(filename);

  float best_ropelength = ropelength(best), new_ropelength;
  int improvements = 0;
  cout << best_ropelength << " ============= " << NODES
       << " " << EPSILON << endl;

  for (int i=0;i<ITERATIONS;++i) {
    cook(fk,EPSILON);
    new_ropelength = ropelength(fk);
//    cout << i << " " << new_ropelength << endl;
    if (new_ropelength < best_ropelength) {
      gradient_flow(&fk);
      best = fk;
      best_ropelength = new_ropelength;
      improvements += 1;
      cout << " +++++++++++++ " << improvements << endl;
    }
    else {
      // restore best
      fk = best;
//      cout << " - ";
    }
  }
  cout << "rope=" << best_ropelength << ", success=" << improvements
       << ", rate=" << (float)improvements/(float)ITERATIONS 
       << " XXX=" << best_ropelength - 16.3719 << endl;
  // symmetrize(best);
  // XXX normalize best!!!
  ofstream of(filename);
  of << setprecision(16) << best;
  of.close();
  dump(best,"test.pkf");
}

int main(int argc, char** argv) {
  if (argc!=2) {
    cout << "Usage : " << argv[0] << " <generic_coeff_file>\n";
    exit(0);
  }
  init();
//  improve("mycoeffs.txt");
  float T = 0.001, C = 1e-5, stop = 1e-12;
  anneal(T,C,stop,argv[1]);
  return 0;
}
