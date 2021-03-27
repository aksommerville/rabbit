#include "rabbit/rb_internal.h"
#include "rabbit/rb_inmap.h"
#include "rabbit/rb_input.h"
#include "rabbit/rb_inmgr.h"
#include "rabbit/rb_text.h"
#include "rabbit/rb_serial.h"

/* New.
 */
 
struct rb_inmap_template *rb_inmap_template_new() {
  struct rb_inmap_template *tm=calloc(1,sizeof(struct rb_inmap_template));
  if (!tm) return 0;
  
  tm->refc=1;
  
  return tm;
}

/* Delete.
 */
 
void rb_inmap_template_del(struct rb_inmap_template *tm) {
  if (!tm) return;
  if (tm->refc-->1) return;
  if (tm->name) free(tm->name);
  if (tm->fieldv) free(tm->fieldv);
  free(tm);
}

/* Retain.
 */
 
int rb_inmap_template_ref(struct rb_inmap_template *tm) {
  if (!tm) return -1;
  if (tm->refc<1) return -1;
  if (tm->refc==INT_MAX) return -1;
  tm->refc++;
  return 0;
}

/* Match description.
 */
 
int rb_inmap_template_match(
  const struct rb_inmap_template *tm,
  const struct rb_input_device_description *desc
) {
  if (tm->vid&&(tm->vid!=desc->vid)) return 0;
  if (tm->pid&&(tm->pid!=desc->pid)) return 0;
  if (tm->namec) {
    if (!rb_pattern_match(tm->name,tm->namec,desc->name,desc->namec)) return 0;
  }
  return 1;
}

/* Add fields to runner, from a template field.
 */
 
int rb_inmap_template_add_field_to_runner(
  struct rb_inmap *inmap,
  int mode,int dstbtnid,
  int btnid,int lo,int hi
) {
  switch (mode) {

    case RB_INMAP_MODE_TWOSTATE: {
        if (lo>=hi) return 0;
        int thresh=(lo+hi)>>1;
        if (thresh<=lo) thresh=lo+1;
        struct rb_inmap_field *rfld=rb_inmap_insert(inmap,-1,btnid);
        if (!rfld) return -1;
        rfld->srclo=thresh;
        rfld->srchi=INT_MAX;
        rfld->dstbtnid=dstbtnid;
      } break;

    case RB_INMAP_MODE_THREEWAY: 
    case RB_INMAP_MODE_YAWEERHT: {
        int range=hi-lo+1;
        if (range<3) return 0;
        
        int mid=(lo+hi)>>1;
        int midlo=(mid+lo)>>1;
        int midhi=(mid+hi)>>1;
        if (midlo>=mid) midlo=mid-1;
        if (midhi<=mid) midhi=mid+1;
        
        struct rb_inmap_field *rfld;
        if (!(rfld=rb_inmap_insert(inmap,-1,btnid))) return -1;
        rfld->srclo=INT_MIN;
        rfld->srchi=midlo;
        if (mode==RB_INMAP_MODE_YAWEERHT) {
          rfld->dstbtnid=dstbtnid&(RB_BTNID_RIGHT|RB_BTNID_DOWN);
        } else {
          rfld->dstbtnid=dstbtnid&(RB_BTNID_LEFT|RB_BTNID_UP);
        }
        
        if (!(rfld=rb_inmap_insert(inmap,-1,btnid))) return -1;
        rfld->srclo=midhi;
        rfld->srchi=INT_MAX;
        if (mode==RB_INMAP_MODE_YAWEERHT) {
          rfld->dstbtnid=dstbtnid&(RB_BTNID_LEFT|RB_BTNID_RIGHT);
        } else {
          rfld->dstbtnid=dstbtnid&(RB_BTNID_RIGHT|RB_BTNID_DOWN);
        }
      } break;
      
    case RB_INMAP_MODE_HAT: {
        if (hi-lo!=7) return 0;
        struct rb_inmap_field *rfld=rb_inmap_insert(inmap,-1,btnid);
        if (!rfld) return -1;
        rfld->srclo=lo;
        rfld->srchi=hi;
        rfld->dstbtnid=RB_BTNID_DPAD;
      } break;
      
  }
  return 0;
}

/* Instantiate.
 */
 
struct rb_inmap_instantiate_context {
  struct rb_inmap_template *tm;
  struct rb_inmap *inmap;
};

static int rb_inmap_instantiate_cb(
  int btnid,uint32_t hidusage,int value,int lo,int hi,void *userdata
) {
  struct rb_inmap_instantiate_context *context=userdata;
  
  int p=rb_inmap_template_search(context->tm,btnid);
  if (p<0) return 0;
  const struct rb_inmap_template_field *tfld=context->tm->fieldv+p;
  
  if (rb_inmap_template_add_field_to_runner(
    context->inmap,tfld->mode,tfld->dstbtnid,btnid,lo,hi
  )<0) return -1;
  
  return 0;
}

struct rb_inmap *rb_inmap_template_instantiate(
  struct rb_inmap_template *tm,
  struct rb_input *input,
  int devix
) {
  struct rb_inmap *inmap=rb_inmap_new();
  if (!inmap) return 0;
  
  struct rb_inmap_instantiate_context context={
    .tm=tm,
    .inmap=inmap,
  };
  
  if (rb_input_device_enumerate(input,devix,rb_inmap_instantiate_cb,&context)<0) {
    rb_inmap_del(inmap);
    return 0;
  }
  
  return inmap;
}

/* Search.
 */
 
int rb_inmap_template_search(const struct rb_inmap_template *tm,int srcbtnid) {
  int lo=0,hi=tm->fieldc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
         if (srcbtnid<tm->fieldv[ck].srcbtnid) hi=ck;
    else if (srcbtnid>tm->fieldv[ck].srcbtnid) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

/* Add field.
 */

int rb_inmap_template_add(struct rb_inmap_template *tm,int srcbtnid,int mode,int dstbtnid) {
  int p=rb_inmap_template_search(tm,srcbtnid);
  if (p>=0) return -1;
  p=-p-1;
  if (tm->fieldc>=tm->fielda) {
    int na=tm->fielda+16;
    if (na>INT_MAX/sizeof(struct rb_inmap_template_field)) return -1;
    void *nv=realloc(tm->fieldv,sizeof(struct rb_inmap_template_field)*na);
    if (!nv) return -1;
    tm->fieldv=nv;
    tm->fielda=na;
  }
  struct rb_inmap_template_field *field=tm->fieldv+p;
  memmove(field+1,field,sizeof(struct rb_inmap_template_field)*(tm->fieldc-p));
  tm->fieldc++;
  field->srcbtnid=srcbtnid;
  field->mode=mode;
  field->dstbtnid=dstbtnid;
  return 0;
}

/* Decode one line.
 * Rules only; we don't do the header or footer.
 */
 
static int rb_inmap_template_decode_line(struct rb_inmap_template *tm,const char *src,int srcc) {
  int srcp=0,tokenc=0;
  const char *token;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  
  // Every valid rule begins with SRCBTNID.
  int srcbtnid;
  token=src+srcp;
  tokenc=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)) { srcp++; tokenc++; }
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  if (rb_int_eval(&srcbtnid,token,tokenc)<0) return -1;
  
  // Next is a keyword identifying the mode: btn, axis, or hat.
  int mode;
  token=src+srcp;
  tokenc=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)) { srcp++; tokenc++; }
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  if ((tokenc==3)&&!memcmp(token,"btn",3)) mode=RB_INMAP_MODE_TWOSTATE;
  else if ((tokenc==4)&&!memcmp(token,"axis",4)) mode=RB_INMAP_MODE_THREEWAY;
  else if ((tokenc==3)&&!memcmp(token,"hat",3)) mode=RB_INMAP_MODE_HAT;
  else return -1;
  
  // btn and axis require one more token for dstbtnid.
  int dstbtnid=0;
  token=src+srcp;
  tokenc=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)) { srcp++; tokenc++; }
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  switch (mode) {
    case RB_INMAP_MODE_TWOSTATE: {
        if ((dstbtnid=rb_input_button_eval(token,tokenc))<0) return -1;
      } break;
    case RB_INMAP_MODE_THREEWAY: {
        if ((tokenc>=1)&&(token[0]=='-')) {
          mode=RB_INMAP_MODE_YAWEERHT;
          token++;
          tokenc--;
        }
        if ((tokenc==4)&&!memcmp(token,"horz",4)) dstbtnid=RB_BTNID_HORZ;
        else if ((tokenc==4)&&!memcmp(token,"vert",4)) dstbtnid=RB_BTNID_VERT;
        else return -1;
      } break;
    case RB_INMAP_MODE_HAT: {
        if (tokenc) return -1;
      } break;
  }
  
  // Assert completion, and add it.
  if (srcp<srcc) return -1;
  if (rb_inmap_template_add(tm,srcbtnid,mode,dstbtnid)<0) return -1;
  
  return 0;
}

/* Decode.
 */
 
int rb_inmap_template_decode(struct rb_inmap_template *tm,const void *src,int srcc) {
  const char *SRC=src;
  int srcp=0;
  while (srcp<srcc) {
    if ((unsigned char)SRC[srcp]<=0x20) { srcp++; continue; }
  
    const char *line=SRC+srcp;
    int linec=0,comment=0;
    while ((srcp<srcc)&&(SRC[srcp]!=0x0a)) {
      if (SRC[srcp]=='#') comment=1;
      srcp++;
      if (!comment) linec++;
    }
    while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
    if (!linec) continue;
    
    if ((linec==3)&&!memcmp(line,"end",3)) break;
    
    if (rb_inmap_template_decode_line(tm,line,linec)<0) {
      fprintf(stderr,"ERROR: Failed to process input config line '%.*s'\n",linec,line);
      return -1;
    }
  }
  return srcp;
}

/* Encode.
 */
 
int rb_inmap_template_encode(struct rb_encoder *dst,const struct rb_inmap_template *tm) {
  if (rb_encode_fmt(dst,"device %d %d \"%.*s\"\n",tm->vid,tm->pid,tm->namec,tm->name)<0) return -1;
  const struct rb_inmap_template_field *field=tm->fieldv;
  int i=tm->fieldc;
  for (;i-->0;field++) {
    switch (field->mode) {
    
      case RB_INMAP_MODE_TWOSTATE: {
          char btnname[32];
          int btnnamec=rb_input_button_repr(btnname,sizeof(btnname),field->dstbtnid);
          if ((btnnamec<0)||(btnnamec>sizeof(btnname))) return -1;
          if (rb_encode_fmt(dst,"  %d btn %.*s\n",field->srcbtnid,btnnamec,btnname)<0) return -1;
        } break;
        
      case RB_INMAP_MODE_THREEWAY:
      case RB_INMAP_MODE_YAWEERHT: {
          if (rb_encode_fmt(dst,
            "  %d axis %s%s\n",
            field->srcbtnid,
            (field->mode==RB_INMAP_MODE_YAWEERHT)?"-":"",
            (field->dstbtnid==RB_BTNID_HORZ)?"horz":"vert"
          )<0) return -1;
        } break;
        
      case RB_INMAP_MODE_HAT: {
          if (rb_encode_fmt(dst,"  %d hat\n",field->srcbtnid)<0) return -1;
        } break;
        
      default: return -1;
    }
  }
  if (rb_encode_raw(dst,"end\n",4)<0) return -1;
  return 0;
}
