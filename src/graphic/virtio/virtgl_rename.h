/* Compile-time renaming of the virtio-gl-hyper fixed-function layer.
 *
 * Widelands brings its own OpenGL implementation -- gl_frontend.cc owns the
 * GL 2.1 ABI it calls -- and that layer implements OpenGL 1.x. Fifteen names
 * exist in both, so linking them together is ambiguous.
 *
 * Rather than fork the layer, this header is forced in front of every one of
 * its translation units with -include, so its entry points link as wlgl_*.
 * gl_frontend.cc keeps the real GL names, which is what Widelands calls, and
 * reaches the layer through these. Not one line of the layer changes, which
 * matters: five other programs run on those exact files.
 *
 * GENERATED from virtgl_gl.h by the rule "every entry point it declares".
 */
#ifndef WL_GRAPHIC_VIRTIO_VIRTGL_RENAME_H
#define WL_GRAPHIC_VIRTIO_VIRTGL_RENAME_H

#define glActiveTextureARB wlgl_glActiveTextureARB
#define glAlphaFunc wlgl_glAlphaFunc
#define glArrayElement wlgl_glArrayElement
#define glBegin wlgl_glBegin
#define glBindTexture wlgl_glBindTexture
#define glBlendFunc wlgl_glBlendFunc
#define glClear wlgl_glClear
#define glClearColor wlgl_glClearColor
#define glClearDepth wlgl_glClearDepth
#define glClearStencil wlgl_glClearStencil
#define glClientActiveTexture wlgl_glClientActiveTexture
#define glClipPlane wlgl_glClipPlane
#define glColor3f wlgl_glColor3f
#define glColor3fv wlgl_glColor3fv
#define glColor3ubv wlgl_glColor3ubv
#define glColor4f wlgl_glColor4f
#define glColor4fv wlgl_glColor4fv
#define glColor4ub wlgl_glColor4ub
#define glColor4ubv wlgl_glColor4ubv
#define glColorMask wlgl_glColorMask
#define glColorMaterial wlgl_glColorMaterial
#define glColorPointer wlgl_glColorPointer
#define glCopyTexImage2D wlgl_glCopyTexImage2D
#define glCopyTexSubImage2D wlgl_glCopyTexSubImage2D
#define glCullFace wlgl_glCullFace
#define glDeleteTextures wlgl_glDeleteTextures
#define glDepthFunc wlgl_glDepthFunc
#define glDepthMask wlgl_glDepthMask
#define glDepthRange wlgl_glDepthRange
#define glDisable wlgl_glDisable
#define glDisableClientState wlgl_glDisableClientState
#define glDrawArrays wlgl_glDrawArrays
#define glDrawBuffer wlgl_glDrawBuffer
#define glDrawElements wlgl_glDrawElements
#define glEnable wlgl_glEnable
#define glEnableClientState wlgl_glEnableClientState
#define glEnd wlgl_glEnd
#define glFinish wlgl_glFinish
#define glFlush wlgl_glFlush
#define glFogf wlgl_glFogf
#define glFogfv wlgl_glFogfv
#define glFogi wlgl_glFogi
#define glFrontFace wlgl_glFrontFace
#define glFrustum wlgl_glFrustum
#define glGenTextures wlgl_glGenTextures
#define glGetBooleanv wlgl_glGetBooleanv
#define glGetError wlgl_glGetError
#define glGetFloatv wlgl_glGetFloatv
#define glGetIntegerv wlgl_glGetIntegerv
#define glGetString wlgl_glGetString
#define glGetTexImage wlgl_glGetTexImage
#define glHint wlgl_glHint
#define glIsEnabled wlgl_glIsEnabled
#define glLightModelf wlgl_glLightModelf
#define glLightModelfv wlgl_glLightModelfv
#define glLightf wlgl_glLightf
#define glLightfv wlgl_glLightfv
#define glLineWidth wlgl_glLineWidth
#define glLoadIdentity wlgl_glLoadIdentity
#define glLoadMatrixf wlgl_glLoadMatrixf
#define glMaterialf wlgl_glMaterialf
#define glMaterialfv wlgl_glMaterialfv
#define glMatrixMode wlgl_glMatrixMode
#define glMultMatrixf wlgl_glMultMatrixf
#define glMultiTexCoord2fARB wlgl_glMultiTexCoord2fARB
#define glNormal3f wlgl_glNormal3f
#define glNormal3fv wlgl_glNormal3fv
#define glOrtho wlgl_glOrtho
#define glPixelStorei wlgl_glPixelStorei
#define glPointSize wlgl_glPointSize
#define glPolygonMode wlgl_glPolygonMode
#define glPolygonOffset wlgl_glPolygonOffset
#define glPopMatrix wlgl_glPopMatrix
#define glPushMatrix wlgl_glPushMatrix
#define glReadBuffer wlgl_glReadBuffer
#define glReadPixels wlgl_glReadPixels
#define glRotatef wlgl_glRotatef
#define glScalef wlgl_glScalef
#define glScissor wlgl_glScissor
#define glShadeModel wlgl_glShadeModel
#define glStencilFunc wlgl_glStencilFunc
#define glStencilMask wlgl_glStencilMask
#define glStencilOp wlgl_glStencilOp
#define glTexCoord2d wlgl_glTexCoord2d
#define glTexCoord2f wlgl_glTexCoord2f
#define glTexCoord2fv wlgl_glTexCoord2fv
#define glTexCoordPointer wlgl_glTexCoordPointer
#define glTexEnvf wlgl_glTexEnvf
#define glTexGenfv wlgl_glTexGenfv
#define glTexGeni wlgl_glTexGeni
#define glTexImage2D wlgl_glTexImage2D
#define glTexParameterf wlgl_glTexParameterf
#define glTexParameteri wlgl_glTexParameteri
#define glTexSubImage2D wlgl_glTexSubImage2D
#define glTranslatef wlgl_glTranslatef
#define glVertex2d wlgl_glVertex2d
#define glVertex2f wlgl_glVertex2f
#define glVertex3d wlgl_glVertex3d
#define glVertex3f wlgl_glVertex3f
#define glVertex3fv wlgl_glVertex3fv
#define glVertexPointer wlgl_glVertexPointer
#define glViewport wlgl_glViewport
#define gluBuild2DMipmaps wlgl_gluBuild2DMipmaps
#define gluPerspective wlgl_gluPerspective
#define virtglEndFrameCapture wlgl_virtglEndFrameCapture
#define virtglPrepareGlowPresent wlgl_virtglPrepareGlowPresent
#define virtglPreparePresent wlgl_virtglPreparePresent

#endif  /* WL_GRAPHIC_VIRTIO_VIRTGL_RENAME_H */
