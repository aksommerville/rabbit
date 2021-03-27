#include "rabbit/rb_internal.h"
#include "rabbit/rb_inmap.h"
#include "rabbit/rb_inmgr.h"
#include "rabbit/rb_input.h"

// friended from rb_inmap_template.c
int rb_inmap_template_add_field_to_runner(
  struct rb_inmap *inmap,
  int mode,int dstbtnid,
  int btnid,int lo,int hi
);

#define RB_ASSESSMENT_UNCHECKED 0 /* we don't know anything yet */
#define RB_ASSESSMENT_DISCARD   1 /* don't use this field */
#define RB_ASSESSMENT_AXIS      2 /* horz or vert, not yet decided */
#define RB_ASSESSMENT_BUTTON    3 /* 2-state but unassigned */
#define RB_ASSESSMENT_PRINCIPAL 4 /* a,b,c,d */
#define RB_ASSESSMENT_AUXILIARY 5 /* select,start */
#define RB_ASSESSMENT_FINAL     100 /* values >=this are a final decision */
#define RB_ASSESSMENT_HAT       100
#define RB_ASSESSMENT_HORZ      101
#define RB_ASSESSMENT_VERT      102
#define RB_ASSESSMENT_SINGLE    200 /* +btnid */

/* Map synthesizer context.
 */
 
struct rb_inmap_synthesizer_field {
  int btnid;
  uint32_t hidusage;
  int value,lo,hi;
  int assessment;
};
 
struct rb_inmap_synthesizer {
  struct rb_inmap_synthesizer_field *fieldv;
  int fieldc,fielda;
  
  // Feature counts, populated midway.
  // We don't count the 2-state buttons, just a mask of which have at least one assignment.
  int axisc;
  int buttonc;
  int principalc;
  int auxiliaryc;
  int hatc;
  int horzc;
  int vertc;
  uint16_t buttons;
  
  uint16_t next_button; // transient, during assignment
  uint16_t next_principal;
  uint16_t next_auxiliary;
  
  struct rb_inmap_template *tm;
  struct rb_inmap *inmap;
};

static void rb_inmap_synthesizer_cleanup(struct rb_inmap_synthesizer *syn) {
  if (syn->fieldv) free(syn->fieldv);
  if (syn->tm) rb_inmap_template_del(syn->tm);
  if (syn->inmap) rb_inmap_del(syn->inmap);
}

/* Field list.
 */
 
static int rb_inmap_synthesizer_search(const struct rb_inmap_synthesizer *syn,int btnid) {
  // I expect devices to return btnid in order, at least it makes sense to.
  // That would be worst case for a binary search, so pick it off special.
  if (syn->fieldc<1) return -1;
  if (btnid>syn->fieldv[syn->fieldc-1].btnid) return -syn->fieldc-1;
  int lo=0,hi=syn->fieldc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
         if (btnid<syn->fieldv[ck].btnid) hi=ck;
    else if (btnid>syn->fieldv[ck].btnid) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

static struct rb_inmap_synthesizer_field *rb_inmap_synthesizer_insert(
  struct rb_inmap_synthesizer *syn,
  int p,
  int btnid
) {
  if (p<0) {
    p=rb_inmap_synthesizer_search(syn,btnid);
    if (p>=0) return 0;
    p=-p-1;
  }
  if ((p<0)||(p>syn->fieldc)) return 0;
  if (p&&(btnid<=syn->fieldv[p-1].btnid)) return 0;
  if ((p<syn->fieldc)&&(btnid>=syn->fieldv[p].btnid)) return 0;
  
  if (syn->fieldc>=syn->fielda) {
    int na=syn->fielda+32;
    if (na>INT_MAX/sizeof(struct rb_inmap_synthesizer_field)) return 0;
    void *nv=realloc(syn->fieldv,sizeof(struct rb_inmap_synthesizer_field)*na);
    if (!nv) return 0;
    syn->fieldv=nv;
    syn->fielda=na;
  }
  
  struct rb_inmap_synthesizer_field *field=syn->fieldv+p;
  memmove(field+1,field,sizeof(struct rb_inmap_synthesizer_field)*(syn->fieldc-p));
  syn->fieldc++;
  memset(field,0,sizeof(struct rb_inmap_synthesizer_field));
  field->btnid=btnid;
  return field;
}

/* Enumerate button callback.
 */
 
static int rb_inmap_cb_button(
  int btnid,uint32_t hidusage,int value,int lo,int hi,void *userdata
) {
  struct rb_inmap_synthesizer *syn=userdata;
  
  // Take an initial stab at it. Can we rule it out immediately?
  int assessment=RB_ASSESSMENT_UNCHECKED;
  if (lo>=hi) { // invalid range, drop it
    return 0;
  } else if (lo==hi-7) { // 8 possible values strongly suggests a hat
    assessment=RB_ASSESSMENT_HAT;
  } else if (lo==hi-1) { // 2 possible values can only be a button
    assessment=RB_ASSESSMENT_BUTTON;
  } else if ((lo<0)&&(hi>0)) { // range straddling zero strongly suggests a 2-way axis
    assessment=RB_ASSESSMENT_AXIS;
  } else if ((lo<value)&&(hi>value)&&(lo<hi-2)) { // current in the middle of a largeish range: axis
    assessment=RB_ASSESSMENT_AXIS;
  }
  
  int p=rb_inmap_synthesizer_search(syn,btnid);
  if (p>=0) return 0; // duplicate, that's weird, ignore it.
  p=-p-1;
  struct rb_inmap_synthesizer_field *field=rb_inmap_synthesizer_insert(syn,p,btnid);
  if (!field) return -1;
  
  field->hidusage=hidusage;
  field->value=value;
  field->lo=lo;
  field->hi=hi;
  field->assessment=assessment;
  
  return 0;
}

/* Fill in any missing assessments, not necessarily final.
 * This will dig deep into hidusage but will not consider cross-field relationships.
 */
 
static void rb_inmap_synthesizer_assess_all(struct rb_inmap_synthesizer *syn) {
  struct rb_inmap_synthesizer_field *field=syn->fieldv;
  int i=syn->fieldc;
  for (;i-->0;field++) {
    if (field->assessment==RB_ASSESSMENT_DISCARD) continue;
    if (field->assessment>=RB_ASSESSMENT_FINAL) continue;
    int range=field->hi-field->lo+1;
    
    // If it's a usage code we recognize, consider range and either assign or discard it.
    #define HORZ if (range>=3) field->assessment=RB_ASSESSMENT_HORZ; else field->assessment=RB_ASSESSMENT_DISCARD; continue;
    #define VERT if (range>=3) field->assessment=RB_ASSESSMENT_VERT; else field->assessment=RB_ASSESSMENT_DISCARD; continue;
    #define SINGLE(tag) field->assessment=RB_ASSESSMENT_SINGLE+RB_BTNID_##tag; continue;
    switch (field->hidusage) {
      case 0x00010030: HORZ // X
      case 0x00010031: VERT // Y
      case 0x00010032: HORZ // Z
      case 0x00010033: HORZ // Rx
      case 0x00010034: VERT // Ry
      case 0x00010035: VERT // Rz
      // Hat Switch: If we didn't pick it off already, forget it
      case 0x00010039: field->assessment=RB_ASSESSMENT_DISCARD; continue;
      case 0x0001003d: SINGLE(START)
      case 0x0001003e: SINGLE(SELECT)
      case 0x00010040: HORZ // Vx
      case 0x00010041: VERT // Vy
      case 0x00010043: HORZ // Vbrx
      case 0x00010044: VERT // Vbry
      case 0x00010090: SINGLE(UP) // Dpad...
      case 0x00010091: SINGLE(DOWN)
      case 0x00010092: SINGLE(RIGHT)
      case 0x00010093: SINGLE(LEFT)
      case 0x00050021: HORZ // Turn Right/Left
      case 0x00050022: VERT // Pitch Forward/Backward
      case 0x00050023: HORZ // Roll Right/Left
      case 0x00050024: HORZ // Move Right/Left
      case 0x00050025: VERT // Move Forward/Backward
      case 0x00050026: VERT // Move Up/Down
      case 0x00050027: HORZ // Lean Right/Left
      case 0x00050028: VERT // Lean Forward/Backward
      case 0x0005002d: field->assessment=RB_ASSESSMENT_AUXILIARY; continue; // New Game
      case 0x00050037: field->assessment=RB_ASSESSMENT_BUTTON; continue; // Gamepad Fire/Jump
      case 0x00050039: field->assessment=RB_ASSESSMENT_BUTTON; continue; // Gamepad Trigger
      case 0x00070006: SINGLE(C) // C
      case 0x00070007: SINGLE(DOWN) // D
      case 0x00070008: SINGLE(UP) // E
      case 0x00070009: SINGLE(RIGHT) // F
      case 0x0007000d: SINGLE(A) // J
      case 0x0007000e: SINGLE(B) // K
      case 0x0007000f: SINGLE(C) // L
      case 0x00070016: SINGLE(LEFT) // S
      case 0x00070019: SINGLE(D) // V
      case 0x0007001b: SINGLE(B) // X
      case 0x0007001d: SINGLE(A) // Z
      case 0x00070028: SINGLE(START) // Enter
      case 0x0007002a: SINGLE(R) // Backspace
      case 0x0007002b: SINGLE(SELECT) // Tab
      case 0x0007002c: SINGLE(A) // Space
      case 0x00070033: SINGLE(D) // Semicolon
      case 0x00070035: SINGLE(L) // Grave
      case 0x0007004f: SINGLE(RIGHT) // Right Arrow
      case 0x00070050: SINGLE(LEFT) // Left Arrow
      case 0x00070051: SINGLE(DOWN) // Down Arrow
      case 0x00070052: SINGLE(UP) // Up Arrow
      case 0x00070054: SINGLE(START) // KP Slash
      case 0x00070055: SINGLE(SELECT) // KP Star
      case 0x00070056: SINGLE(START) // KP Dash
      case 0x00070057: SINGLE(C) // KP Plus
      case 0x00070058: SINGLE(B) // KP Enter
      case 0x0007005a: SINGLE(DOWN) // KP 2
      case 0x0007005c: SINGLE(LEFT) // KP 4
      case 0x0007005d: SINGLE(DOWN) // KP 5
      case 0x0007005e: SINGLE(RIGHT) // KP 6
      case 0x0007005f: SINGLE(L) // KP 7
      case 0x00070060: SINGLE(UP) // KP 8
      case 0x00070061: SINGLE(R) // KP 9
      case 0x00070062: SINGLE(A) // KP 0
      case 0x00070063: SINGLE(D) // KP Dot
    }
    #undef HORZ
    #undef VERT
    #undef SINGLE
    
    // Discard anything else on the keyboard page.
    // There could be dozens of these and it's silly to map all of them.
    if ((field->hidusage&0xffff0000)==0x00070000) {
      field->assessment=RB_ASSESSMENT_DISCARD;
      continue;
    }
    
    // If the range straddles zero, call it an axis. Otherwise button.
    if ((field->lo<0)&&(field->hi>0)) {
      field->assessment=RB_ASSESSMENT_AXIS;
    } else {
      field->assessment=RB_ASSESSMENT_BUTTON;
    }
  }
}

/* Drop any DISCARD assessments.
 * Drop UNCHECKED too; they should not have gotten this far.
 */
 
static void rb_inmap_synthesizer_drop_discards(struct rb_inmap_synthesizer *syn) {
  int i=syn->fieldc-1;
  struct rb_inmap_synthesizer_field *field=syn->fieldv+i;
  for (;i-->0;field--) {
    if (
      (field->assessment==RB_ASSESSMENT_DISCARD)||
      (field->assessment==RB_ASSESSMENT_UNCHECKED)
    ) {
      syn->fieldc--;
      memmove(field,field+1,sizeof(struct rb_inmap_synthesizer_field)*(syn->fieldc-i));
    }
  }
}

/* Count the various assessments.
 */
 
static void rb_inmap_synthesizer_count_assessments(struct rb_inmap_synthesizer *syn) {
  struct rb_inmap_synthesizer_field *field=syn->fieldv;
  int i=syn->fieldc;
  for (;i-->0;field++) switch (field->assessment) {
    case RB_ASSESSMENT_AXIS: syn->axisc++; break;
    case RB_ASSESSMENT_BUTTON: syn->buttonc++; break;
    case RB_ASSESSMENT_PRINCIPAL: syn->principalc++; break;
    case RB_ASSESSMENT_AUXILIARY: syn->auxiliaryc++; break;
    case RB_ASSESSMENT_HAT: syn->hatc++; syn->buttons|=RB_BTNID_DPAD; break;
    case RB_ASSESSMENT_HORZ: syn->horzc++; syn->buttons|=RB_BTNID_HORZ; break;
    case RB_ASSESSMENT_VERT: syn->vertc++; syn->buttons|=RB_BTNID_VERT; break;
    default: {
        int btnid=field->assessment-RB_ASSESSMENT_SINGLE;
        if ((btnid>0)&&(btnid<0x10000)) {
          syn->buttons|=btnid;
        }
      }
  }
}

/* Return an assessment for the next generic button, and update shared state.
 */
 
static int rb_inmap_synthesizer_next_button(struct rb_inmap_synthesizer *syn) {
  uint16_t all_buttons=0x0fff;
  if (!syn->next_button) syn->next_button=0x0010;
  
  // If we have axes or hats, the directional buttons don't participate.
  if (
    ((syn->buttons&RB_BTNID_DPAD)==RB_BTNID_DPAD)||
    syn->hatc||
    (syn->horzc&&syn->vertc)||
    (syn->axisc>=2)
  ) all_buttons&=~RB_BTNID_DPAD;
  
  // If one is missing, advance next_button until we find it.
  if (syn->buttons!=all_buttons) {
    int panic=16;
    while (panic-->0) {
      int assessment=0;
      if (!(syn->buttons&syn->next_button)) {
        assessment=RB_ASSESSMENT_SINGLE+syn->next_button;
        syn->buttons|=syn->next_button;
      }
      syn->next_button<<=1;
      if (syn->next_button>0xfff) {
        if (all_buttons&0xf) syn->next_button=1;
        else syn->next_button=0x10;
      }
      if (assessment) return assessment;
    }
  }
  
  // We're duplicating buttons now, which is ok.
  int assessment=RB_ASSESSMENT_SINGLE+syn->next_button;
  syn->next_button<<=1;
  if (syn->next_button>0xfff) {
    if (all_buttons&0xf) syn->next_button=1;
    else syn->next_button=0x10;
  }
  return assessment;
}
  

/* With counts taken, make a final assessment of each field.
 */
 
static void rb_inmap_synthesizer_finalize_assessments(struct rb_inmap_synthesizer *syn) {
  struct rb_inmap_synthesizer_field *field=syn->fieldv;
  int i=syn->fieldc;
  for (;i-->0;field++) switch (field->assessment) {
  
    case RB_ASSESSMENT_AXIS: {
        syn->axisc--;
        if (!(syn->buttons&RB_BTNID_HORZ)) {
          field->assessment=RB_ASSESSMENT_HORZ;
          syn->horzc++;
          syn->buttons|=RB_BTNID_HORZ;
        } else if (!(syn->buttons&RB_BTNID_VERT)) {
          field->assessment=RB_ASSESSMENT_VERT;
          syn->vertc++;
          syn->buttons|=RB_BTNID_VERT;
        } else if (syn->horzc<=syn->vertc) {
          field->assessment=RB_ASSESSMENT_HORZ;
          syn->horzc++;
          syn->buttons|=RB_BTNID_HORZ;
        } else {
          field->assessment=RB_ASSESSMENT_VERT;
          syn->vertc++;
          syn->buttons|=RB_BTNID_VERT;
        }
      } break;
      
    case RB_ASSESSMENT_BUTTON: {
        syn->buttonc--;
        field->assessment=rb_inmap_synthesizer_next_button(syn);
      } break;
      
    case RB_ASSESSMENT_PRINCIPAL: {
        syn->principalc--;
             if (!(syn->buttons&RB_BTNID_A)) { field->assessment=RB_ASSESSMENT_SINGLE+RB_BTNID_A; syn->buttons|=RB_BTNID_A; }
        else if (!(syn->buttons&RB_BTNID_B)) { field->assessment=RB_ASSESSMENT_SINGLE+RB_BTNID_B; syn->buttons|=RB_BTNID_B; }
        else if (!(syn->buttons&RB_BTNID_C)) { field->assessment=RB_ASSESSMENT_SINGLE+RB_BTNID_C; syn->buttons|=RB_BTNID_C; }
        else if (!(syn->buttons&RB_BTNID_D)) { field->assessment=RB_ASSESSMENT_SINGLE+RB_BTNID_D; syn->buttons|=RB_BTNID_D; }
        else {
          if (!syn->next_principal) syn->next_principal=RB_BTNID_A;
          field->assessment=RB_ASSESSMENT_SINGLE+syn->next_principal;
          syn->next_principal<<=1;
          if (syn->next_principal>RB_BTNID_D) syn->next_principal=RB_BTNID_A;
        }
      } break;
      
    case RB_ASSESSMENT_AUXILIARY: {
        syn->auxiliaryc--;
             if (!(syn->buttons&RB_BTNID_START)) { field->assessment=RB_ASSESSMENT_SINGLE+RB_BTNID_START; syn->buttons|=RB_BTNID_START; }
        else if (!(syn->buttons&RB_BTNID_SELECT)) { field->assessment=RB_ASSESSMENT_SINGLE+RB_BTNID_SELECT; syn->buttons|=RB_BTNID_SELECT; }
        else if (syn->next_auxiliary==RB_BTNID_SELECT) { field->assessment=RB_ASSESSMENT_SINGLE+RB_BTNID_SELECT; syn->next_auxiliary=RB_BTNID_START; }
        else { field->assessment=RB_ASSESSMENT_SINGLE+RB_BTNID_START; syn->next_auxiliary=RB_BTNID_SELECT; }
      } break;
      
  }
}

/* All assessments are now final.
 * Return nonzero if we have enough and should create a template.
 */
 
static int rb_inmap_synthesizer_assess(const struct rb_inmap_synthesizer *syn) {
  const uint16_t required=
    RB_BTNID_DPAD|
    RB_BTNID_A|
    RB_BTNID_B|
    RB_BTNID_START|
  0; // Optional: C,D,L,R,SELECT
  if ((syn->buttons&required)==required) return 1;
  return 0;
}

/* Set matching criteria on the template.
 */
 
static int rb_inmap_synthesizer_set_criteria(
  struct rb_inmap_synthesizer *syn,
  const struct rb_input_device_description *desc
) {
  syn->tm->vid=desc->vid;
  syn->tm->pid=desc->pid;
  if (desc->namec>0) {
    if (!(syn->tm->name=malloc(desc->namec+1))) return -1;
    memcpy(syn->tm->name,desc->name,desc->namec);
    syn->tm->name[desc->namec]=0;
    syn->tm->namec=desc->namec;
  }
  return 0;
}

/* Populate template.
 */
 
static int rb_inmap_synthesizer_populate_template(struct rb_inmap_synthesizer *syn) {

  // Template will have exactly as many fields as the synthesizer, and they're already in order.
  // So we cheat this a little.
  syn->tm->fieldv=malloc(sizeof(struct rb_inmap_template_field)*syn->fieldc);
  if (!syn->tm->fieldv) return -1;
  syn->tm->fieldc=syn->fieldc;
  syn->tm->fielda=syn->fieldc;
  
  struct rb_inmap_template_field *dst=syn->tm->fieldv;
  const struct rb_inmap_synthesizer_field *src=syn->fieldv;
  int i=syn->fieldc;
  for (;i-->0;src++,dst++) {
    dst->srcbtnid=src->btnid;
    if (src->assessment>=RB_ASSESSMENT_SINGLE) {
      dst->mode=RB_INMAP_MODE_TWOSTATE;
      dst->dstbtnid=src->assessment-RB_ASSESSMENT_SINGLE;
    } else switch (src->assessment) {
      case RB_ASSESSMENT_HAT: {
          dst->mode=RB_INMAP_MODE_HAT;
          dst->dstbtnid=RB_BTNID_DPAD;
        } break;
      case RB_ASSESSMENT_HORZ: {
          dst->mode=RB_INMAP_MODE_THREEWAY;
          dst->dstbtnid=RB_BTNID_HORZ;
        } break;
      case RB_ASSESSMENT_VERT: {
          dst->mode=RB_INMAP_MODE_THREEWAY;
          dst->dstbtnid=RB_BTNID_VERT;
        } break;
      default: return -1;
    }
  }

  return 0;
}

/* Populate live map if present.
 */
 
static int rb_inmap_synthesizer_populate_inmap(struct rb_inmap_synthesizer *syn) {
  if (!syn->inmap) return 0;
  
  // Unlike with template, runners might get multiple fields per synthesizer field.
  // So do it the proper inefficient way.
  const struct rb_inmap_synthesizer_field *src=syn->fieldv;
  int i=syn->fieldc;
  for (;i-->0;src++) {
    switch (src->assessment) {
      case RB_ASSESSMENT_HAT: if (rb_inmap_template_add_field_to_runner(
          syn->inmap,RB_INMAP_MODE_HAT,RB_BTNID_DPAD,src->btnid,src->lo,src->hi
        )<0) return -1; break;
      case RB_ASSESSMENT_HORZ: if (rb_inmap_template_add_field_to_runner(
          syn->inmap,RB_INMAP_MODE_THREEWAY,RB_BTNID_HORZ,src->btnid,src->lo,src->hi
        )<0) return -1; break;
      case RB_ASSESSMENT_VERT: if (rb_inmap_template_add_field_to_runner(
          syn->inmap,RB_INMAP_MODE_THREEWAY,RB_BTNID_VERT,src->btnid,src->lo,src->hi
        )<0) return -1; break;
      default: {
          int btnid=src->assessment-RB_ASSESSMENT_SINGLE;
          if ((btnid<0)||(btnid>0xffff)) return -1;
          if (rb_inmap_template_add_field_to_runner(
            syn->inmap,RB_INMAP_MODE_TWOSTATE,btnid,src->btnid,src->lo,src->hi
          )<0) return -1;
        } break;
    }
  }
  return 0;
}

/* Synthesize template.
 */
 
struct rb_inmap_template *rb_inmap_template_synthesize(
  struct rb_input *input,
  int devix,
  const struct rb_input_device_description *desc,
  struct rb_inmap **instance
) {
  struct rb_inmap_synthesizer syn={
  };
  if (rb_input_device_enumerate(input,devix,rb_inmap_cb_button,&syn)<0) {
    rb_inmap_synthesizer_cleanup(&syn);
    return 0;
  }
  
  rb_inmap_synthesizer_assess_all(&syn);
  rb_inmap_synthesizer_drop_discards(&syn);
  rb_inmap_synthesizer_count_assessments(&syn);
  rb_inmap_synthesizer_finalize_assessments(&syn);
  
  if (!rb_inmap_synthesizer_assess(&syn)) {
    rb_inmap_synthesizer_cleanup(&syn);
    return 0;
  }
  
  // We're doing it, so create the objects.
  if (!(syn.tm=rb_inmap_template_new())) {
    rb_inmap_synthesizer_cleanup(&syn);
    return 0;
  }
  if (instance) {
    if (!(syn.inmap=rb_inmap_new())) {
      rb_inmap_synthesizer_cleanup(&syn);
      return 0;
    }
  }
  
  if (desc) {
    if (rb_inmap_synthesizer_set_criteria(&syn,desc)<0) {
      rb_inmap_synthesizer_cleanup(&syn);
      return 0;
    }
  }
  
  if (
    (rb_inmap_synthesizer_populate_template(&syn)<0)||
    (rb_inmap_synthesizer_populate_inmap(&syn)<0)
  ) {
    rb_inmap_synthesizer_cleanup(&syn);
    return 0;
  }
  
  struct rb_inmap_template *tm=syn.tm;
  if (instance) *instance=syn.inmap;
  syn.tm=0;
  syn.inmap=0;
  rb_inmap_synthesizer_cleanup(&syn);
  return tm;
}
