

// winrott.h

// husk at rette i winrott.c

// Widest screen: 640..MAXSCREENWIDTH wide at 480 high (the rotation effect's
// buffer is 1024 wide)
#define MAXSCREENWIDTH 1024
#define IS_WIDE_RES(w, h) ((h) == 480 && (w) >= 640 && (w) <= MAXSCREENWIDTH && (w) % 16 == 0)

extern int iGLOBAL_SCREENWIDTH;	 // bna
extern int iGLOBAL_SCREENHEIGHT; // bna
extern int iGLOBAL_SCREENBWIDE;
extern int iG_SCREENWIDTH; // default screen width in bytes

extern int iGLOBAL_HEALTH_X;
extern int iGLOBAL_HEALTH_Y;
extern int iGLOBAL_AMMO_X;
extern int iGLOBAL_AMMO_Y;

extern int iGLOBAL_FOCALWIDTH;
extern double dGLOBAL_FPFOCALWIDTH;

void EnableScreenStretch(void);
void DisableScreenStretch(void);
