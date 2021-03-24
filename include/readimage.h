typedef int readimagefn(Readimage_Iobuf*, Array**, String**, int);
extern readimagefn readgifdata;
extern readimagefn readjpegdata;
extern void	remap(Readimage_Rawimage*, Draw_Display*, int, Draw_Image**, String**);
