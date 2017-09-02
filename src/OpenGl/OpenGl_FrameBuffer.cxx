// Created by: Kirill GAVRILOV
// Copyright (c) 2011-2014 OPEN CASCADE SAS
//
// This file is part of Open CASCADE Technology software library.
//
// This library is free software; you can redistribute it and/or modify it under
// the terms of the GNU Lesser General Public License version 2.1 as published
// by the Free Software Foundation, with special exception defined in the file
// OCCT_LGPL_EXCEPTION.txt. Consult the file LICENSE_LGPL_21.txt included in OCCT
// distribution for complete text of the license and disclaimer of any warranty.
//
// Alternatively, this file may be used under the terms of Open CASCADE
// commercial license or contractual agreement.

#include <OpenGl_FrameBuffer.hxx>
#include <OpenGl_ArbFBO.hxx>

#include <Standard_Assert.hxx>
#include <TCollection_ExtendedString.hxx>

IMPLEMENT_STANDARD_RTTIEXT(OpenGl_FrameBuffer,OpenGl_Resource)

namespace
{

  //! Determine data type from texture sized format.
  static bool getDepthDataFormat (GLint   theTextFormat,
                                  GLenum& thePixelFormat,
                                  GLenum& theDataType)
  {
    switch (theTextFormat)
    {
      case GL_DEPTH24_STENCIL8:
      {
        thePixelFormat = GL_DEPTH_STENCIL;
        theDataType    = GL_UNSIGNED_INT_24_8;
        return true;
      }
      case GL_DEPTH32F_STENCIL8:
      {
        thePixelFormat = GL_DEPTH_STENCIL;
        theDataType    = GL_FLOAT_32_UNSIGNED_INT_24_8_REV;
        return true;
      }
      case GL_DEPTH_COMPONENT16:
      {
        thePixelFormat = GL_DEPTH_COMPONENT;
        theDataType    = GL_UNSIGNED_SHORT;
        return true;
      }
      case GL_DEPTH_COMPONENT24:
      {
        thePixelFormat = GL_DEPTH_COMPONENT;
        theDataType    = GL_UNSIGNED_INT;
        return true;
      }
      case GL_DEPTH_COMPONENT32F:
      {
        thePixelFormat = GL_DEPTH_COMPONENT;
        theDataType    = GL_FLOAT;
        return true;
      }
    }
    return false;
  }

}

// =======================================================================
// function : OpenGl_FrameBuffer
// purpose  :
// =======================================================================
OpenGl_FrameBuffer::OpenGl_FrameBuffer()
: myVPSizeX (0),
  myVPSizeY (0),
  myNbSamples (0),
  myColorFormat (GL_RGBA8),
  myDepthFormat (GL_DEPTH24_STENCIL8),
  myGlFBufferId (NO_FRAMEBUFFER),
  myGlColorRBufferId (NO_RENDERBUFFER),
  myGlDepthRBufferId (NO_RENDERBUFFER),
  myIsOwnBuffer  (false),
  myColorTexture (new OpenGl_Texture()),
  myDepthStencilTexture (new OpenGl_Texture())
{
  //
}

// =======================================================================
// function : ~OpenGl_FrameBuffer
// purpose  :
// =======================================================================
OpenGl_FrameBuffer::~OpenGl_FrameBuffer()
{
  Release (NULL);
}

// =======================================================================
// function : Init
// purpose  :
// =======================================================================
Standard_Boolean OpenGl_FrameBuffer::Init (const Handle(OpenGl_Context)& theGlContext,
                                           const GLsizei   theSizeX,
                                           const GLsizei   theSizeY,
                                           const GLint     theColorFormat,
                                           const GLint     theDepthFormat,
                                           const GLsizei   theNbSamples)
{
  myColorFormat = theColorFormat;
  myDepthFormat = theDepthFormat;
  myNbSamples   = theNbSamples;
  if (theGlContext->arbFBO == NULL)
  {
    return Standard_False;
  }

  // clean up previous state
  Release (theGlContext.operator->());
  if (myColorFormat == 0
   && myDepthFormat == 0)
  {
    return Standard_False;
  }

  myIsOwnBuffer = true;

  // setup viewport sizes as is
  myVPSizeX = theSizeX;
  myVPSizeY = theSizeY;
  const Standard_Integer aSizeX = theSizeX > 0 ? theSizeX : 2;
  const Standard_Integer aSizeY = theSizeY > 0 ? theSizeY : 2;

  // Create the textures (will be used as color buffer and depth-stencil buffer)
  if (theNbSamples != 0)
  {
    if (myColorFormat != 0
    && !myColorTexture       ->Init2DMultisample (theGlContext, theNbSamples, myColorFormat, aSizeX, aSizeY))
    {
      Release (theGlContext.operator->());
      return Standard_False;
    }
    if (myDepthFormat != 0
    && !myDepthStencilTexture->Init2DMultisample (theGlContext, theNbSamples, myDepthFormat, aSizeX, aSizeY))
    {
      Release (theGlContext.operator->());
      return Standard_False;
    }
  }
  else
  {
    if (myColorFormat != 0
    && !myColorTexture->Init (theGlContext, myColorFormat,
                              GL_RGBA, GL_UNSIGNED_BYTE,
                              aSizeX, aSizeY, Graphic3d_TOT_2D))
    {
      Release (theGlContext.operator->());
      return Standard_False;
    }

    // extensions (GL_OES_packed_depth_stencil, GL_OES_depth_texture) + GL version might be used to determine supported formats
    // instead of just trying to create such texture
    GLenum aPixelFormat = 0;
    GLenum aDataType    = 0;
    if (myDepthFormat != 0
    &&  getDepthDataFormat (myDepthFormat, aPixelFormat, aDataType)
    && !myDepthStencilTexture->Init (theGlContext, myDepthFormat,
                                      aPixelFormat, aDataType,
                                      aSizeX, aSizeY, Graphic3d_TOT_2D))
    {
      TCollection_ExtendedString aMsg = TCollection_ExtendedString()
        + "Warning! Depth textures are not supported by hardware!";
      theGlContext->PushMessage (GL_DEBUG_SOURCE_APPLICATION,
                                 GL_DEBUG_TYPE_PORTABILITY,
                                 0,
                                 GL_DEBUG_SEVERITY_HIGH,
                                 aMsg);

      theGlContext->arbFBO->glGenRenderbuffers (1, &myGlDepthRBufferId);
      theGlContext->arbFBO->glBindRenderbuffer (GL_RENDERBUFFER, myGlDepthRBufferId);
      theGlContext->arbFBO->glRenderbufferStorage (GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, aSizeX, aSizeY);
      theGlContext->arbFBO->glBindRenderbuffer (GL_RENDERBUFFER, NO_RENDERBUFFER);
    }
  }

  // Build FBO and setup it as texture
  theGlContext->arbFBO->glGenFramebuffers (1, &myGlFBufferId);
  theGlContext->arbFBO->glBindFramebuffer (GL_FRAMEBUFFER, myGlFBufferId);
  if (myColorTexture->IsValid())
  {
    theGlContext->arbFBO->glFramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                                  myColorTexture->GetTarget(), myColorTexture->TextureId(), 0);
  }
  if (myDepthStencilTexture->IsValid())
  {
  #ifdef GL_DEPTH_STENCIL_ATTACHMENT
    theGlContext->arbFBO->glFramebufferTexture2D (GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                                                  myDepthStencilTexture->GetTarget(), myDepthStencilTexture->TextureId(), 0);
  #else
    theGlContext->arbFBO->glFramebufferTexture2D (GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                                  myDepthStencilTexture->GetTarget(), myDepthStencilTexture->TextureId(), 0);
    theGlContext->arbFBO->glFramebufferTexture2D (GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
                                                  myDepthStencilTexture->GetTarget(), myDepthStencilTexture->TextureId(), 0);
  #endif
  }
  else if (myGlDepthRBufferId != NO_RENDERBUFFER)
  {
    theGlContext->arbFBO->glFramebufferRenderbuffer (GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                                     GL_RENDERBUFFER, myGlDepthRBufferId);
  }
  if (theGlContext->arbFBO->glCheckFramebufferStatus (GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
  {
    Release (theGlContext.operator->());
    return Standard_False;
  }

  UnbindBuffer (theGlContext);
  return Standard_True;
}

// =======================================================================
// function : Init
// purpose  :
// =======================================================================
Standard_Boolean OpenGl_FrameBuffer::InitLazy (const Handle(OpenGl_Context)& theGlContext,
                                               const GLsizei                 theViewportSizeX,
                                               const GLsizei                 theViewportSizeY,
                                               const GLint                   theColorFormat,
                                               const GLint                   theDepthFormat,
                                               const GLsizei                 theNbSamples)
{
  if (myVPSizeX     == theViewportSizeX
   && myVPSizeY     == theViewportSizeY
   && myColorFormat == theColorFormat
   && myDepthFormat == theDepthFormat
   && myNbSamples   == theNbSamples)
  {
    return IsValid();
  }

  return Init (theGlContext, theViewportSizeX, theViewportSizeY, theColorFormat, theDepthFormat, theNbSamples);
}

// =======================================================================
// function : InitWithRB
// purpose  :
// =======================================================================
Standard_Boolean OpenGl_FrameBuffer::InitWithRB (const Handle(OpenGl_Context)& theGlCtx,
                                                 const GLsizei                 theSizeX,
                                                 const GLsizei                 theSizeY,
                                                 const GLint                   theColorFormat,
                                                 const GLint                   theDepthFormat,
                                                 const GLuint                  theColorRBufferFromWindow)
{
  myColorFormat = theColorFormat;
  myDepthFormat = theDepthFormat;
  myNbSamples   = 0;
  if (theGlCtx->arbFBO == NULL)
  {
    return Standard_False;
  }

  // clean up previous state
  Release (theGlCtx.operator->());

  myIsOwnBuffer = true;

  // setup viewport sizes as is
  myVPSizeX = theSizeX;
  myVPSizeY = theSizeY;
  const Standard_Integer aSizeX = theSizeX > 0 ? theSizeX : 2;
  const Standard_Integer aSizeY = theSizeY > 0 ? theSizeY : 2;

  // Create the render-buffers
  if (theColorRBufferFromWindow != NO_RENDERBUFFER)
  {
    myGlColorRBufferId = theColorRBufferFromWindow;
  }
  else if (myColorFormat != 0)
  {
    theGlCtx->arbFBO->glGenRenderbuffers (1, &myGlColorRBufferId);
    theGlCtx->arbFBO->glBindRenderbuffer (GL_RENDERBUFFER, myGlColorRBufferId);
    theGlCtx->arbFBO->glRenderbufferStorage (GL_RENDERBUFFER, myColorFormat, aSizeX, aSizeY);
  }

  if (myDepthFormat != 0)
  {
    theGlCtx->arbFBO->glGenRenderbuffers (1, &myGlDepthRBufferId);
    theGlCtx->arbFBO->glBindRenderbuffer (GL_RENDERBUFFER, myGlDepthRBufferId);
    theGlCtx->arbFBO->glRenderbufferStorage (GL_RENDERBUFFER, myDepthFormat, aSizeX, aSizeY);
    theGlCtx->arbFBO->glBindRenderbuffer (GL_RENDERBUFFER, NO_RENDERBUFFER);
  }

  // create FBO
  theGlCtx->arbFBO->glGenFramebuffers (1, &myGlFBufferId);
  theGlCtx->arbFBO->glBindFramebuffer (GL_FRAMEBUFFER, myGlFBufferId);
  theGlCtx->arbFBO->glFramebufferRenderbuffer (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                               GL_RENDERBUFFER, myGlColorRBufferId);
  if (myGlDepthRBufferId != NO_RENDERBUFFER)
  {
  #ifdef GL_DEPTH_STENCIL_ATTACHMENT
    theGlCtx->arbFBO->glFramebufferRenderbuffer (GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                                                 GL_RENDERBUFFER, myGlDepthRBufferId);
  #else
    theGlCtx->arbFBO->glFramebufferRenderbuffer (GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                                 GL_RENDERBUFFER, myGlDepthRBufferId);
    theGlCtx->arbFBO->glFramebufferRenderbuffer (GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
                                                 GL_RENDERBUFFER, myGlDepthRBufferId);
  #endif
  }
  if (theGlCtx->arbFBO->glCheckFramebufferStatus (GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
  {
    UnbindBuffer (theGlCtx);
    Release (theGlCtx.operator->());
    return Standard_False;
  }

  UnbindBuffer (theGlCtx);
  return Standard_True;
}

// =======================================================================
// function : InitWrapper
// purpose  :
// =======================================================================
Standard_Boolean OpenGl_FrameBuffer::InitWrapper (const Handle(OpenGl_Context)& theGlCtx)
{
  myNbSamples = 0;
  if (theGlCtx->arbFBO == NULL)
  {
    return Standard_False;
  }

  // clean up previous state
  Release (theGlCtx.operator->());

  GLint anFbo = GLint(NO_FRAMEBUFFER);
  ::glGetIntegerv (GL_FRAMEBUFFER_BINDING, &anFbo);
  if (anFbo == GLint(NO_FRAMEBUFFER))
  {
    return Standard_False;
  }

  GLint aColorType = 0;
  GLint aColorId   = 0;
  GLint aDepthType = 0;
  GLint aDepthId   = 0;
  theGlCtx->arbFBO->glGetFramebufferAttachmentParameteriv (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &aColorType);
  theGlCtx->arbFBO->glGetFramebufferAttachmentParameteriv (GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,  GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &aDepthType);

  myGlFBufferId = GLuint(anFbo);
  myIsOwnBuffer = false;
  if (aColorType == GL_RENDERBUFFER)
  {
    theGlCtx->arbFBO->glGetFramebufferAttachmentParameteriv (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &aColorId);
    myGlColorRBufferId = aColorId;
  }
  else if (aColorType != GL_NONE)
  {
    TCollection_ExtendedString aMsg = "OpenGl_FrameBuffer::InitWrapper(), color attachment of unsupported type has been skipped!";
    theGlCtx->PushMessage (GL_DEBUG_SOURCE_APPLICATION,
                           GL_DEBUG_TYPE_ERROR,
                           0,
                           GL_DEBUG_SEVERITY_HIGH,
                           aMsg);
  }

  if (aDepthType == GL_RENDERBUFFER)
  {
    theGlCtx->arbFBO->glGetFramebufferAttachmentParameteriv (GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,  GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &aDepthId);
    myGlDepthRBufferId = aDepthId;
  }
  else if (aDepthType != GL_NONE)
  {
    TCollection_ExtendedString aMsg = "OpenGl_FrameBuffer::InitWrapper(), depth attachment of unsupported type has been skipped!";
    theGlCtx->PushMessage (GL_DEBUG_SOURCE_APPLICATION,
                           GL_DEBUG_TYPE_ERROR,
                           0,
                           GL_DEBUG_SEVERITY_HIGH,
                           aMsg);
  }

  // retrieve dimensions
  GLuint aRBuffer = myGlColorRBufferId != NO_RENDERBUFFER ? myGlColorRBufferId : myGlDepthRBufferId;
  if (aRBuffer != NO_RENDERBUFFER)
  {
    theGlCtx->arbFBO->glBindRenderbuffer (GL_RENDERBUFFER, aRBuffer);
    theGlCtx->arbFBO->glGetRenderbufferParameteriv (GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH,  &myVPSizeX);
    theGlCtx->arbFBO->glGetRenderbufferParameteriv (GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &myVPSizeY);
    theGlCtx->arbFBO->glBindRenderbuffer (GL_RENDERBUFFER, NO_RENDERBUFFER);
  }

  return aRBuffer != NO_RENDERBUFFER;
}

// =======================================================================
// function : Release
// purpose  :
// =======================================================================
void OpenGl_FrameBuffer::Release (OpenGl_Context* theGlCtx)
{
  if (isValidFrameBuffer())
  {
    // application can not handle this case by exception - this is bug in code
    Standard_ASSERT_RETURN (theGlCtx != NULL,
      "OpenGl_FrameBuffer destroyed without GL context! Possible GPU memory leakage...",);
    if (theGlCtx->IsValid()
     && myIsOwnBuffer)
    {
      theGlCtx->arbFBO->glDeleteFramebuffers (1, &myGlFBufferId);
      if (myGlColorRBufferId != NO_RENDERBUFFER)
      {
        theGlCtx->arbFBO->glDeleteRenderbuffers (1, &myGlColorRBufferId);
      }
      if (myGlDepthRBufferId != NO_RENDERBUFFER)
      {
        theGlCtx->arbFBO->glDeleteRenderbuffers (1, &myGlDepthRBufferId);
      }
    }
    myGlFBufferId      = NO_FRAMEBUFFER;
    myGlColorRBufferId = NO_RENDERBUFFER;
    myGlDepthRBufferId = NO_RENDERBUFFER;
    myIsOwnBuffer      = false;
  }

  myColorTexture->Release (theGlCtx);
  myDepthStencilTexture->Release (theGlCtx);

  myVPSizeX = 0;
  myVPSizeY = 0;
}

// =======================================================================
// function : SetupViewport
// purpose  :
// =======================================================================
void OpenGl_FrameBuffer::SetupViewport (const Handle(OpenGl_Context)& theGlCtx)
{
  const Standard_Integer aViewport[4] = { 0, 0, myVPSizeX, myVPSizeY };
  theGlCtx->ResizeViewport (aViewport);
}

// =======================================================================
// function : ChangeViewport
// purpose  :
// =======================================================================
void OpenGl_FrameBuffer::ChangeViewport (const GLsizei theVPSizeX,
                                         const GLsizei theVPSizeY)
{
  myVPSizeX = theVPSizeX;
  myVPSizeY = theVPSizeY;
}

// =======================================================================
// function : BindBuffer
// purpose  :
// =======================================================================
void OpenGl_FrameBuffer::BindBuffer (const Handle(OpenGl_Context)& theGlCtx)
{
  theGlCtx->arbFBO->glBindFramebuffer (GL_FRAMEBUFFER, myGlFBufferId);
}

// =======================================================================
// function : BindDrawBuffer
// purpose  :
// =======================================================================
void OpenGl_FrameBuffer::BindDrawBuffer (const Handle(OpenGl_Context)& theGlCtx)
{
  theGlCtx->arbFBO->glBindFramebuffer (GL_DRAW_FRAMEBUFFER, myGlFBufferId);
}

// =======================================================================
// function : BindReadBuffer
// purpose  :
// =======================================================================
void OpenGl_FrameBuffer::BindReadBuffer (const Handle(OpenGl_Context)& theGlCtx)
{
  theGlCtx->arbFBO->glBindFramebuffer (GL_READ_FRAMEBUFFER, myGlFBufferId);
}

// =======================================================================
// function : UnbindBuffer
// purpose  :
// =======================================================================
void OpenGl_FrameBuffer::UnbindBuffer (const Handle(OpenGl_Context)& theGlCtx)
{
  if (!theGlCtx->DefaultFrameBuffer().IsNull()
   &&  theGlCtx->DefaultFrameBuffer().operator->() != this)
  {
    theGlCtx->DefaultFrameBuffer()->BindBuffer (theGlCtx);
  }
  else
  {
    theGlCtx->arbFBO->glBindFramebuffer (GL_FRAMEBUFFER, NO_FRAMEBUFFER);
  }
}
