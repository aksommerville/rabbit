#include "drmgx_internal.h"

/* GLSL.
 */
 
static const char drmgx_vshader[]=
  "precision mediump float;\n"
  "attribute vec2 aposition;\n"
  "attribute vec2 atexcoord;\n"
  "varying vec2 vtexcoord;\n"
  "void main() {\n"
  "  gl_Position=vec4(aposition,0.0,1.0);\n"
  "  vtexcoord=atexcoord;\n"
  "}\n"
"";

static const char drmgx_fshader[]=
  "precision mediump float;\n"
  "uniform sampler2D sampler;\n"
  "varying vec2 vtexcoord;\n"
  "void main() {\n"
  "  gl_FragColor=texture2D(sampler,vtexcoord);\n"
  "}\n"
"";

/* Compile shader.
 */
 
static int drmgx_render_compile(struct drmgx *drmgx,const char *src,int srcc,int type) {

  int glslversion=drmgx->glslversion;
  if (!glslversion) glslversion=100;
  char version[256];
  int versionc;
  versionc=snprintf(version,sizeof(version),"#version %d\n",glslversion);
  if ((versionc<1)||(versionc>=sizeof(version))) return -1;
  const char *srcv[2]={version,src};
  GLint lenv[2]={versionc,srcc};

  GLuint id=glCreateShader(type);
  if (!id) return -1;
  glShaderSource(id,2,srcv,lenv);
  glCompileShader(id);

  GLint status=0;
  glGetShaderiv(id,GL_COMPILE_STATUS,&status);
  if (status) {
    glAttachShader(drmgx->programid,id);
    glDeleteShader(id);
    return 0;
  }

  int err=-1;
  GLint loga=0;
  glGetShaderiv(id,GL_INFO_LOG_LENGTH,&loga);
  if ((loga>0)&&(loga<INT_MAX)) {
    char *log=malloc(loga);
    if (log) {
      GLint logc=0;
      glGetShaderInfoLog(id,loga,&logc,log);
      while (logc&&((unsigned char)log[logc-1]<=0x20)) logc--;
      if ((logc>0)&&(logc<=loga)) {
        fprintf(stderr,"Error compiling %s shader:\n%.*s\n",(type==GL_VERTEX_SHADER)?"vertex":"fragment",logc,log);
        err=-2;
      }
      free(log);
    }
  }
  glDeleteShader(id);
  return err;
}

/* Link shader.
 */
 
static int drmgx_render_link(struct drmgx *drmgx) {

  glLinkProgram(drmgx->programid);
  GLint status=0;
  glGetProgramiv(drmgx->programid,GL_LINK_STATUS,&status);
  if (status) return 0;

  /* Link failed. */
  int err=-1;
  GLint loga=0;
  glGetProgramiv(drmgx->programid,GL_INFO_LOG_LENGTH,&loga);
  if ((loga>0)&&(loga<INT_MAX)) {
    char *log=malloc(loga);
    if (log) {
      GLint logc=0;
      glGetProgramInfoLog(drmgx->programid,loga,&logc,log);
      while (logc&&((unsigned char)log[logc-1]<=0x20)) logc--;
      if ((logc>0)&&(logc<=loga)) {
        fprintf(stderr,"Error linking shader:\n%.*s\n",logc,log);
        err=-2;
      }
      free(log);
    }
  }
  return err;
}

/* Initialize shader.
 */
 
static int drmgx_render_init_shader(struct drmgx *drmgx) {

  if (!(drmgx->programid=glCreateProgram())) return -1;
  if (drmgx_render_compile(drmgx,drmgx_vshader,sizeof(drmgx_vshader)-1,GL_VERTEX_SHADER)<0) return -1;
  if (drmgx_render_compile(drmgx,drmgx_fshader,sizeof(drmgx_fshader)-1,GL_FRAGMENT_SHADER)<0) return -1;
  
  glBindAttribLocation(drmgx->programid,0,"aposition");
  glBindAttribLocation(drmgx->programid,1,"atexcoord");
  
  if (drmgx_render_link(drmgx)<0) return -1;

  return 0;
}

/* Quit.
 */
 
void drmgx_render_quit(struct drmgx *drmgx) {
  if (drmgx->fbcvt) free(drmgx->fbcvt);
  if (drmgx->texid) glDeleteTextures(1,&drmgx->texid);
  if (drmgx->programid) glDeleteProgram(drmgx->programid);
}

/* Init.
 */
 
int drmgx_render_init(struct drmgx *drmgx) {
  
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
  
  glGenTextures(1,&drmgx->texid);
  if (!drmgx->texid) {
    glGenTextures(1,&drmgx->texid);
    if (!drmgx->texid) return -1;
  }
  glBindTexture(GL_TEXTURE_2D,drmgx->texid);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,drmgx->reqfilter?GL_LINEAR:GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,drmgx->reqfilter?GL_LINEAR:GL_NEAREST);
  
  if (drmgx_render_init_shader(drmgx)<0) return -1;
  
  return 0;
}

/* 24-bit RGB from TINY8.
 */
 
static void drmgx_fbcvt_RGB_TINY8(uint8_t *dst,const uint8_t *src,int w,int h) {
  int pxc=w*h;
  for (;pxc-->0;dst+=3,src++) {
    //TODO i've coded this blindly from memory and it's probably wrong
    dst[0]=(*src)&0x03;
    dst[0]|=dst[0]<<2;
    dst[0]|=dst[0]<<4;
    dst[1]=((*src)<<3)&0xe0;
    dst[1]|=(dst[1]>>3);
    dst[1]|=(dst[1]>>6);
    dst[2]=(*src)&0xe0;
    dst[2]|=(dst[2]>>3);
    dst[3]|=(dst[2]>>6);
  }
}

/* 24-bit RGB from TINY16.
 */
 
static void drmgx_fbcvt_RGB_TINY16(uint8_t *dst,const uint8_t *src,int w,int h) {
  int pxc=w*h;
  for (;pxc-->0;dst+=3,src+=2) {
    dst[2]=((src[0]&0xf8));
    dst[1]=((src[0]&0x07)<<5)|((src[1]&0xe0)>>3);
    dst[0]=((src[1]&0x1f)<<3);
  }
}

/* 24-bit RGB from whatever the hell it is Rabbit uses.
 */

static void drmgx_fbcvt_RGB_RGBX(uint8_t *dst,const uint32_t *src,int w,int h) {
  int pxc=w*h;
  for (;pxc-->0;dst+=3,src++) {
    dst[0]=(*src);
    dst[1]=(*src)>>8;
    dst[2]=(*src)>>16;
  }
}

/* Pick new output bounds.
 */
 
static void drmgx_recalculate_output_bounds(struct drmgx *drmgx,int w,int h) {
  int dstw=(w*drmgx->h)/h;
  if (dstw<=drmgx->w) { // match screen height
    drmgx->dstr_right=(GLfloat)dstw/(GLfloat)drmgx->w;
    drmgx->dstr_top=1.0f;
  } else { // match screen width
    int dsth=(h*drmgx->w)/w;
    drmgx->dstr_right=1.0f;
    drmgx->dstr_top=(GLfloat)dsth/(GLfloat)drmgx->h;
  }
  drmgx->dstr_left=-drmgx->dstr_right;
  drmgx->dstr_bottom=-drmgx->dstr_top;
}

/* Allocate fbcvt if we don't have it.
 */
 
static int drmgx_fbcvt_require(struct drmgx *drmgx,int w,int h) {
  if (drmgx->fbcvt) return 0;
  if (!(drmgx->fbcvt=malloc(w*h*3))) return -1;
  return 0;
}

/* Upload and render framebuffer.
 */
 
void drmgx_render(struct drmgx *drmgx,const void *fb,int w,int h,int fmt) {

  if ((w!=drmgx->pvfbw)||(h!=drmgx->pvfbh)) {
    drmgx_recalculate_output_bounds(drmgx,w,h);
    drmgx->pvfbw=w;
    drmgx->pvfbh=h;
    if (drmgx->fbcvt) {
      free(drmgx->fbcvt);
      drmgx->fbcvt=0;
    }
  }

  glBindTexture(GL_TEXTURE_2D,drmgx->texid);
  switch (fmt) {
    case DRMGX_FMT_Y8: glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,w,h,0,GL_LUMINANCE,GL_UNSIGNED_BYTE,fb); break;
    case DRMGX_FMT_TINY8: {
        if (drmgx_fbcvt_require(drmgx,w,h)<0) return;
        drmgx_fbcvt_RGB_TINY8(drmgx->fbcvt,fb,w,h);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,w,h,0,GL_RGB,GL_UNSIGNED_BYTE,drmgx->fbcvt);
      } break;
    case DRMGX_FMT_TINY16: {
        if (drmgx_fbcvt_require(drmgx,w,h)<0) return;
        drmgx_fbcvt_RGB_TINY16(drmgx->fbcvt,fb,w,h);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,w,h,0,GL_RGB,GL_UNSIGNED_BYTE,drmgx->fbcvt);
      } break;
    case DRMGX_FMT_RGB: glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,w,h,0,GL_RGB,GL_UNSIGNED_BYTE,fb); break;
    case DRMGX_FMT_RGBX: {
        if (drmgx_fbcvt_require(drmgx,w,h)<0) return;
        drmgx_fbcvt_RGB_RGBX(drmgx->fbcvt,fb,w,h);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,w,h,0,GL_RGB,GL_UNSIGNED_BYTE,drmgx->fbcvt);
      } break;
  }

  glClearColor(0.0f,0.25f,0.0f,1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  
  struct { GLfloat x,y; } positionv[]={
    {drmgx->dstr_left,drmgx->dstr_top},
    {drmgx->dstr_left,drmgx->dstr_bottom},
    {drmgx->dstr_right,drmgx->dstr_top},
    {drmgx->dstr_right,drmgx->dstr_bottom},
  };
  struct { GLubyte x,y; } texcoordv[]={
    {0,0},{0,1},{1,0},{1,1},
  };
  
  glEnable(GL_TEXTURE_2D);
  glUseProgram(drmgx->programid);
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(0,2,GL_FLOAT,0,sizeof(positionv[0]),positionv);
  glVertexAttribPointer(1,2,GL_UNSIGNED_BYTE,0,sizeof(texcoordv[0]),texcoordv);
  glDrawArrays(GL_TRIANGLE_STRIP,0,4);
}
