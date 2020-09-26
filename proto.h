#if defined(__STDC__) || defined(__cplusplus)
# define P_(s) s
#else
# define P_(s) ()
#endif


/* 3dsconv.c */
void usage P_((char *errmsg));
int main P_((int argc, char **argv));
char *change_extension P_((char *name, char *ext));
char *name2label P_((char *));
int write_output_file P_((char *name));

/* 3dsfile.c */
int read3dsfile P_((char *fname));

/* lwfile.c */
int readlwfile P_((char *fname));

/* internal.c */
void AddMaterial P_((Material *mat));
int GetMaterial P_((char *name));
void AddVertex P_((Object *obj, Vertex *vert));
void AddPolygon P_((Object *obj, _Polygon *p));
void CalcFaceNormal P_((Object *obj, _Polygon *P));
void CalcVertexNormals P_((Object *obj));
void MergeVertices P_((Object *obj));
void MergeFaces P_((Object *obj));
void CheckUncoloredFaces P_((Object *obj));
Object *CreateObject P_((char *name));
Object *FindObject P_((char *name));
Object *FixObjectLists P_((void));
Matrix MMult(Matrix A, Matrix B);
Matrix MatInv(Matrix M);
#if !defined(atarist) && !defined(_WIN32)
double rint P_((double));
#endif

/* jagout.c */
int JAGwritefile P_((FILE *f, Object *));

/* n3dout.c */
unsigned rgb2cry P_((int red, int green, int blue));
int N3Dwritefile P_((FILE *f, Object *));

/* cout.c */
int Cwritefile P_((FILE *f, Object *));

/* cfout.c */
int CFwritefile P_((FILE *f, Object *));

/* targa.c */
int read_targa P_((Material *mat, int colrflag ));

#undef P_
