/* SPDX-License-Identifier: MIT
 * Private, bounded diagnostic readback after the existing export glFinish.
 * Geometry and frame-zero calibration must be admitted by the own fixture.
 * An exported frame identity is not proof of a completed display flip.
 */
#include "sffx-frame-id-decode.h"
static pthread_once_t sffx_frame_once=PTHREAD_ONCE_INIT;
static pthread_mutex_t sffx_frame_lock=PTHREAD_MUTEX_INITIALIZER;
static int sffx_frame_geometry[7],sffx_frame_enabled;
static unsigned sffx_frame_count;
static double sffx_frame_started;
static void sffx_frame_configure(void){
    const char *value=getenv("SFFX_FRAME_ID_GEOMETRY");int end=0;
    if(!value||sscanf(value,"%d,%d,%d,%d,%d,%d,%d%n",
       &sffx_frame_geometry[0],&sffx_frame_geometry[1],
       &sffx_frame_geometry[2],&sffx_frame_geometry[3],
       &sffx_frame_geometry[4],&sffx_frame_geometry[5],
       &sffx_frame_geometry[6],&end)!=7||value[end])return;
    int *g=sffx_frame_geometry;
    if(g[0]<1||g[0]>8192||g[1]<1||g[1]>8192||
       g[2]<0||g[3]<0||g[2]>g[0]||g[3]>g[1]||
       g[4]<160||g[4]>g[0]-g[2]||g[5]<90||g[5]>g[1]-g[3]||
       (g[6]!=1&&g[6]!=-1))return;
    sffx_frame_enabled=1;
}
static void sffx_frame_export(CGLContextObj ctx,IOSurfaceRef surface,GLuint fbo){
    pthread_once(&sffx_frame_once,sffx_frame_configure);
    int *g=sffx_frame_geometry;
    if(!sffx_frame_enabled||ctx->width!=g[0]||ctx->height!=g[1])return;
    pthread_mutex_lock(&sffx_frame_lock);
    double before=pd_cgl_now();
    if(!sffx_frame_started)sffx_frame_started=before;
    if(sffx_frame_count>=1200||before-sffx_frame_started>35000){
        pthread_mutex_unlock(&sffx_frame_lock);return;
    }
    unsigned sequence=sffx_frame_count++;
    int left=g[2]+(88*g[4]+960)/1920;
    int right=g[2]+(1816*g[4]+960)/1920,pixels=right-left;
    int y[2];
    for(int row=0;row<2;row++){
        int offset=((row?232:120)*g[5]+540)/1080;
        y[row]=g[6]>0?g[3]+offset:g[3]+g[5]-1-offset;
    }
    unsigned char rows[2][8192*4];
    GLint old_fbo=0,old_buffer=0,pack=0,alignment=0,length=0,skip_rows=0,skip_pixels=0;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&old_fbo);
    glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING,&pack);
    glGetIntegerv(GL_PACK_ALIGNMENT,&alignment);
    glGetIntegerv(GL_PACK_ROW_LENGTH,&length);
    glGetIntegerv(GL_PACK_SKIP_ROWS,&skip_rows);
    glGetIntegerv(GL_PACK_SKIP_PIXELS,&skip_pixels);
    glBindFramebuffer(GL_READ_FRAMEBUFFER,fbo);
    glGetIntegerv(GL_READ_BUFFER,&old_buffer);glReadBuffer(GL_COLOR_ATTACHMENT0);
    glBindBuffer(GL_PIXEL_PACK_BUFFER,0);glPixelStorei(GL_PACK_ALIGNMENT,1);
    glPixelStorei(GL_PACK_ROW_LENGTH,0);glPixelStorei(GL_PACK_SKIP_ROWS,0);
    glPixelStorei(GL_PACK_SKIP_PIXELS,0);
    for(int row=0;row<2;row++)glReadPixels(left,y[row],pixels,1,GL_RGBA,GL_UNSIGNED_BYTE,rows[row]);
    unsigned error=glGetError();
    glReadBuffer((GLenum)old_buffer);glBindFramebuffer(GL_READ_FRAMEBUFFER,(GLuint)old_fbo);
    glBindBuffer(GL_PIXEL_PACK_BUFFER,(GLuint)pack);glPixelStorei(GL_PACK_ALIGNMENT,alignment);
    glPixelStorei(GL_PACK_ROW_LENGTH,length);glPixelStorei(GL_PACK_SKIP_ROWS,skip_rows);
    glPixelStorei(GL_PACK_SKIP_PIXELS,skip_pixels);
    double after=pd_cgl_now();
    int frame=error?-1:sf_media_frame_id_rgba(rows[0],rows[1],(unsigned)pixels);
    uint32_t id=pd_iosurface.surfaceID?pd_iosurface.surfaceID(surface):0;
    flockfile(stderr);
    fprintf(stderr,"FXFRAMEID pid=%d sequence=%u surface=%u size=%dx%d fbo=%u before_ms=%.6f after_ms=%.6f readback_bytes=%d frame=%d gl_error=%u reason=%s rows=",
        getpid(),sequence,id,ctx->width,ctx->height,fbo,before,after,pixels*8,frame,error,
        error?"gl_error":frame<0?"marker_rejected":"ok");
    if(!error)for(int row=0;row<2;row++)for(unsigned column=0;column<18;column++){
        unsigned x=(2*column+1)*(unsigned)pixels/36,sum=0;
        for(int d=-1;d<=1;d++)for(unsigned c=0;c<3;c++)sum+=rows[row][4*(x+d)+c];
        fprintf(stderr,"%s%u",row||column?",":"",sum/9);
    }
    fputc('\n',stderr);funlockfile(stderr);
    pthread_mutex_unlock(&sffx_frame_lock);
}
