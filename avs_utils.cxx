#include "avs_utils.h"

double ex[DIM] = {1.0, 0.0, 0.0};
double ey[DIM] = {0.0, 1.0, 0.0};
double ez[DIM] = {0.0, 0.0, 0.0};

// Global Data
char AVS_dir[cbuffer];
char Out_dir[cbuffer];
char Out_name[cbuffer];
char Outp_dir[cbuffer];
char Outp_name[cbuffer];

int fluid_veclen;
int particle_veclen;

FILE *fluid_field;
FILE *particle_field;

FILE *fluid_cod;
FILE *particle_cod;

int GTS; 
int Num_snap;

double A;
double A_XI;
double DX;

int NS[DIM];
int &NX = NS[0];
int &NY = NS[1];
int &NZ = NS[2];
int lbox[DIM];

int Nspec;
int Nparticles;
int *p_spec;
double *p_slip;
double *p_slipmode;
JAX *p_axis;

// Per Frame data
//fluid
FILE *fluid_data;
double **u;

//particle
FILE *particle_data;
double UP;
double B1;
double B2;
double B1_app;
double r0[DIM]; 
int r0_int[DIM];
double res0[DIM];
double v0[DIM];
double w0[DIM];
double f0[DIM];
double t0[DIM];
double Q0[DIM][DIM];
quaternion q0;

// read filename from avs files
inline void get_filename(char* line, char*name){
  char dmy_char[256];
  string dmy_string;
  size_t start, end;
  
  dmy_string.assign(line);
  start = dmy_string.find_first_of("=") + 2;
  end = (dmy_string.substr(start)).find_first_of(" ") + start;
  strcpy(name, dmy_string.substr(start, end-start).c_str());
}

void get_system_data(UDFManager *ufin){

  { // particle size
    Location target("constitutive_eq");
    string str;
    ufin->get(target.sub("type"),str);
    target.down(str);
    ufin->get(target.sub("DX"), DX);
    ufin->get("A", A);
    ufin->get("A_XI", A_XI);
    fprintf(stderr, "#DX   : %8.3f\n", DX);
    fprintf(stderr, "#A    : %8.3f\n", A);
    fprintf(stderr, "#A_XI : %8.3f\n", A_XI);
    A *= DX;
    A_XI *= DX;
  }
  { // mesh size
    Location target("mesh");
    int dmy[DIM];
    ufin->get(target.sub("NPX"), dmy[0]);
    ufin->get(target.sub("NPY"), dmy[1]);
    ufin->get(target.sub("NPZ"), dmy[2]);
    for(int d = 0; d < DIM; d++){
      Ns[d] = 1 << dmy[d];
      lbox[d] = Ns[d] * DX;
    }
    fprintf(stderr, "#NS   : %8d %8d %8d\n\n", Ns[0], Ns[1], Ns[2]);
    
  }
  {  // object data
    Location target("object_type");
    string str;
    ufin->get(target.sub("type"), str);
    if(str != "spherical_particle"){
      fprintf(stderr, "Error: only spherical particles supported\n");
      exit_job(EXIT_FAILURE);
    }

    Nspec = ufin->size("object_type.spherical_particle.Particle_spec[]");
    p_axis = (JAX*) malloc(sizeof(JAX) * Nspec);
    p_slip = (double*) malloc(sizeof(double) * Nspec);
    p_slipmode = (double*) malloc(sizeof(double) * Nspec);
    int * dmy_num = (int*)malloc(sizeof(int) * Nspec);
    Nparticles = 0;
    for(int i = 0; i < Nspec; i++){
      char str[256];
      sprintf(str, "object_type.spherical_particle.Particle_spec[%d]", i);
      Location target(str);
      ufin->get(target.sub("Particle_number"), dmy_num[i]);
      Nparticles += dmy_num[i];

      string str2;
      ufin->get(target.sub("janus_axis"), str2);
      if(str2 == "X"){
	sp_axis[i] = x_axis;
      }else if(str2 == "Y"){
	sp_axis[i] = y_axis;
      }else if(str2 == "Z"){
	sp_axis[i] = z_axis;
      }else{
	fprintf(stderr, "Error: Unknown axis specification\n");
	exit_job(EXIT_FAILURE);
      }

      ufin->get(target.sub("janus_propulsion"), str2);
      if(str2 == "SLIP"){
	ufin->get(target.sub("janus_slip_vel"), sp_slip[i]);
	ufin->get(target.sub("janus_slip_mode"), sp_slipmode[i]);
      }
    }
    p_spec = (int*)malloc(sizeof(int) * Nparticles);
    int count = 0;
    for(int i = 0; i < Nspec; i++)
      for(int j = 0; j < dmy_num[i]; j++)
	p_spec[count++] = i;
    assert(count == Nparticles);

    fprintf(stderr, " #species   : %8d\n", Nspec);
    fprintf(stderr, " #particles : %8d\n\n", Nparticles);
    free(dmy_num);
  }
  {  // avs output
    string str;
    Location target("output");
    ufin->get(target.sub("GTS"), GTS);
    ufin->get(target.sub("Num_snap"), Num_snap);
    target.down("ON");
    ufin->get(target.sub("Out_dir"), str);
    strcpy(AVS_dir, str.c_str());
    if(opendir(AVS_dir) == NULL){
      fprintf(stderr, "Error: AVS directory does not exist\n");
      exit_job(EXIT_FAILURE);
    }
    ufin->get(target.sub("Out_name"), str);
    strcpy(Out_name, str.c_str());
    
    ufin->get(target.sub("FileType"), str);
    if(str != "BINARY"){
      fprintf(stderr, "Error: Unknown filetype\n");
      exit_job(EXIT_FAILURE);
    }

    str = "avs";
    strcpy(Out_dir, str.c_str());
    sprintf(Outp_dir, "%s", Out_dir);
    sprintf(Outp_name, "%sp", Out_name);

    fprintf(stderr, " #Avs directory :  %15s\n", AVS_dir);
    fprintf(stderr, " #data directory : %15s %15s \n", Out_dir, Outp_dir);
    fprintf(stderr, " #data filename  : %15s %15s \n\n", Out_name, Outp_name);
  }
}

void initialize(const int &pid){
  char dmy_char[256];
  string dmy_string;
  char* line = NULL;
  size_t len, found;

  //open field data specification file
  sprintf(dmy_char, "%s/%s.fld", AVS_dir, Out_name);
  fluid_field = filecheckopen(dmy_char, "r");
  getline(&line, &len, fluid_field); //Header
  getline(&line, &len, fluid_field); //ndim
  getline(&line, &len, fluid_field); //dim1
  getline(&line, &len, fluid_field); //dim2
  getline(&line, &len, fluid_field); //dim3
  getline(&line, &len, fluid_field); //nspace
  getline(&line, &len, fluid_field); //veclen

  dmy_string.assign(line);
  found = dmy_string.find("=");
  fluid_veclen = atoi(dmy_string.substr(++found).c_str());

  getline(&line, &len, fluid_field); //float data
  getline(&line, &len, fluid_field); //uniform  
  getline(&line, &len, fluid_field); //nstep
  getline(&line, &len, fluid_field); //label

  sprintf(dmy_char, "%s/%s.fld", AVS_dir, Outp_name);
  particle_field = filecheckopen(dmy_char, "r");
  getline(&line, &len, particle_field); //Header
  getline(&line, &len, particle_field); //ndim
  getline(&line, &len, particle_field); //dim1
  getline(&line, &len, particle_field); //nspace
  getline(&line, &len, particle_field); //veclen

  dmy_string.assign(line);
  found = dmy_string.find("=");
  particle_veclen = atoi(dmy_string.substr(++found).c_str());

  getline(&line, &len, particle_field); //float data
  getline(&line, &len, particle_field); //irregular  field
  getline(&line, &len, particle_field); //nstep
  getline(&line, &len, particle_field); //label
  
  // allocate init memory
  u = (double **) malloc(sizeof(double*) * DIM);
  for(int d = 0; d < DIM; d++){
    u[d] = alloc_1d_double(NX*NY*NZ);
  }
  assert(pid < Nparticles);
  if(sp_axis[p_spec[pid]] == x_axis){
    e3 = ex;
    e1 = ey;
    e2 = ez;
  }else if(sp_axis[p_spec[pid]] == y_axis){
    e3 = ey;
    e1 = ez;
    e2 = ex;
  }else if(sp_axis[p_spec[pid]] == z_axis){
    e3 = ez;
    e1 = ex;
    e2 = ey;
  }
  
}

// initialize new avs frame
void setup_avs_frame(){
  //find files for new frame
  size_t len, start, end;
  char* line = NULL;
  string dmy_string;

  // step header
  getline(&line, &len, particle_field);
  getline(&line, &len, fluid_field);

  char dmy_fluid[256], fluid_path[256];
  char dmy_particle[256], particle_path[256];

  // coord file
  for(int i = 0; i < DIM; i++){
    getline(&line, &len, fluid_field);
    get_filename(line, dmy_fluid);

    getline(&line, &len, particle_field); 
    get_filename(line, dmy_particle);
  }
  sprintf(fluid_path, "%s/%s", AVS_dir, dmy_fluid);
  sprintf(particle_path, "%s/%s", AVS_dir, dmy_particle);
  fluid_cod = filecheckopen(fluid_path, "r");                 //particle coordinate file
  particle_cod = filecheckopen(particle_path, "r");


  //INPUT field data file
  for(int i = 0; i < fluid_veclen; i++){
    getline(&line, &len, fluid_field);
    get_filename(line, dmy_fluid);
  }
  getline(&line, &len, fluid_field);//EOT
  sprintf(fluid_path, "%s/%s", AVS_dir, dmy_fluid);
  fluid_data = filecheckopen(fluid_path, "r");                //fluid data file

  for(int i = 0; i < particle_veclen; i++){
    getline(&line, &len, particle_field);
    get_filename(line, dmy_particle);
  }
  getline(&line, &len, particle_field);//EOT
  sprintf(particle_path, "%s/%s", AVS_dir, dmy_particle);
  particle_data = filecheckopen(particle_path, "r");          //particle data file
}

// reset avs frame
void clear_avs_frame(){
  fclose(fluid_cod);
  fclose(particle_cod);

  fclose(fluid_data);
  fclose(particle_data);


  fluid_cod = NULL;
  fluid_data = NULL;
  particle_data = NULL;
}