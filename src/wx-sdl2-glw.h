#ifndef SRC_WX_SDL2_GLW_H_
#define SRC_WX_SDL2_GLW_H_

PFNGLACTIVETEXTUREPROC _wrap__glActiveTexture = 0;
PFNGLCREATESHADERPROC glCreateShader = 0;
PFNGLSHADERSOURCEPROC glShaderSource = 0;
PFNGLCOMPILESHADERPROC glCompileShader = 0;
PFNGLGETSHADERIVPROC glGetShaderiv = 0;
PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog = 0;
PFNGLCREATEPROGRAMPROC glCreateProgram = 0;
PFNGLATTACHSHADERPROC glAttachShader = 0;
PFNGLLINKPROGRAMPROC glLinkProgram = 0;
PFNGLGETPROGRAMIVPROC glGetProgramiv = 0;
PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog = 0;
PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation = 0;
PFNGLGETATTRIBLOCATIONPROC glGetAttribLocation = 0;
PFNGLUSEPROGRAMPROC glUseProgram = 0;
PFNGLGENERATEMIPMAPPROC glGenerateMipmap = 0;
PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers = 0;
PFNGLDELETESHADERPROC glDeleteShader = 0;
PFNGLDELETEPROGRAMPROC glDeleteProgram = 0;
PFNGLDELETEBUFFERSPROC glDeleteBuffers = 0;
PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays = 0;
PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers = 0;
PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer = 0;
PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D = 0;
PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus = 0;
PFNGLGENVERTEXARRAYSPROC glGenVertexArrays = 0;
PFNGLBINDVERTEXARRAYPROC glBindVertexArray = 0;
PFNGLGENBUFFERSPROC glGenBuffers = 0;
PFNGLBINDBUFFERPROC glBindBuffer = 0;
PFNGLBUFFERDATAPROC glBufferData = 0;
PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer = 0;
PFNGLUNIFORM1IPROC glUniform1i = 0;
PFNGLUNIFORM1FPROC glUniform1f = 0;
PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray = 0;
PFNGLUNIFORMMATRIX4FVPROC glUniformMatrix4fv = 0;
PFNGLUNIFORM2FVPROC glUniform2fv = 0;
PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray = 0;
PFNGLBUFFERSUBDATAPROC glBufferSubData = 0;

GLAPI void GLAPIENTRY glActiveTexture( GLenum texture )
{
	_wrap__glActiveTexture(texture);
}


void glw_init()
{
        _wrap__glActiveTexture = SDL_GL_GetProcAddress("glActiveTexture");
        glCreateShader = SDL_GL_GetProcAddress("glCreateShader");
        glShaderSource = SDL_GL_GetProcAddress("glShaderSource");
        glCompileShader = SDL_GL_GetProcAddress("glCompileShader");
        glGetShaderiv = SDL_GL_GetProcAddress("glGetShaderiv");
        glGetShaderInfoLog = SDL_GL_GetProcAddress("glGetShaderInfoLog");
        glCreateProgram = SDL_GL_GetProcAddress("glCreateProgram");
        glAttachShader = SDL_GL_GetProcAddress("glAttachShader");
        glLinkProgram = SDL_GL_GetProcAddress("glLinkProgram");
        glGetProgramiv = SDL_GL_GetProcAddress("glGetProgramiv");
        glGetProgramInfoLog = SDL_GL_GetProcAddress("glGetProgramInfoLog");
        glGetUniformLocation = SDL_GL_GetProcAddress("glGetUniformLocation");
        glGetAttribLocation = SDL_GL_GetProcAddress("glGetAttribLocation");
        glUseProgram = SDL_GL_GetProcAddress("glUseProgram");
        glGenerateMipmap = SDL_GL_GetProcAddress("glGenerateMipmap");
        glDeleteFramebuffers = SDL_GL_GetProcAddress("glDeleteFramebuffers");
        glDeleteShader = SDL_GL_GetProcAddress("glDeleteShader");
        glDeleteProgram = SDL_GL_GetProcAddress("glDeleteProgram");
        glDeleteBuffers = SDL_GL_GetProcAddress("glDeleteBuffers");
        glDeleteVertexArrays = SDL_GL_GetProcAddress("glDeleteVertexArrays");
        glGenFramebuffers = SDL_GL_GetProcAddress("glGenFramebuffers");
        glBindFramebuffer = SDL_GL_GetProcAddress("glBindFramebuffer");
        glFramebufferTexture2D = SDL_GL_GetProcAddress("glFramebufferTexture2D");
        glCheckFramebufferStatus = SDL_GL_GetProcAddress("glCheckFramebufferStatus");
        glGenVertexArrays = SDL_GL_GetProcAddress("glGenVertexArrays");
        glBindVertexArray = SDL_GL_GetProcAddress("glBindVertexArray");
        glGenBuffers = SDL_GL_GetProcAddress("glGenBuffers");
        glBindBuffer = SDL_GL_GetProcAddress("glBindBuffer");
        glBufferData = SDL_GL_GetProcAddress("glBufferData");
        glVertexAttribPointer = SDL_GL_GetProcAddress("glVertexAttribPointer");
        glUniform1i = SDL_GL_GetProcAddress("glUniform1i");
        glUniform1f = SDL_GL_GetProcAddress("glUniform1f");
        glEnableVertexAttribArray = SDL_GL_GetProcAddress("glEnableVertexAttribArray");
        glUniformMatrix4fv = SDL_GL_GetProcAddress("glUniformMatrix4fv");
        glUniform2fv = SDL_GL_GetProcAddress("glUniform2fv");
        glDisableVertexAttribArray = SDL_GL_GetProcAddress("glDisableVertexAttribArray");
        glBufferSubData = SDL_GL_GetProcAddress("glBufferSubData");
}



#endif /* SRC_WX_SDL2_GLW_H_ */
