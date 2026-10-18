

// winrott.h

// husk at rette i winrott.c

// Screens: 640..1024 wide at 480 high (Hor+), and taller ones up to
// 1920x1080 (at least 4:3, a multiple of 16 wide and of 8 high)
#define MAXSCREENWIDTH 1920
#define MAXSCREENHEIGHT 1080
#define IS_WIDE_RES(w, h)                                                     \
	((w) % 16 == 0 &&                                                         \
	 (((h) == 480 && (w) >= 640 && (w) <= 1024) ||                            \
	  ((h) > 480 && (h) <= MAXSCREENHEIGHT && (h) % 8 == 0 &&                 \
	   (w) * 3 >= (h) * 4 && (w) <= MAXSCREENWIDTH)))

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
