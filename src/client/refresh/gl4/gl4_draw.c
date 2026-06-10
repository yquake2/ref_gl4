/*
 * Copyright (C) 1997-2001 Id Software, Inc.
 * Copyright (C) 2016-2017 Daniel Gibson
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * =======================================================================
 *
 * Drawing of all images that are not textures
 *
 * =======================================================================
 */

#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include "header/local.h"

unsigned d_8to24table[256];

gl4image_t *draw_chars;
GLint oldViewPort[4];

static GLuint vbo2D = 0, vao2D = 0, vao2Dcolor = 0; // vao2D is for textured rendering, vao2Dcolor for color-only
static GLuint bloomTex[2] = {0,0};
static GLuint bloomFBO[2] = {0,0};
static qboolean bloomInitialized = false;

void
GL4_Draw_InitLocal(void)
{
	/* load console characters */
	draw_chars = R_FindPic("conchars", (findimage_t)GL4_FindImage);
	if (!draw_chars)
	{
		ri.Sys_Error(ERR_FATAL, "%s: Couldn't load pics/conchars.pcx",
			__func__);
	}

	// set up attribute layout for 2D textured rendering
	glGenVertexArrays(1, &vao2D);
	glBindVertexArray(vao2D);

	glGenBuffers(1, &vbo2D);
	GL4_BindVBO(vbo2D);

	GL4_UseProgram(gl4state.si2D.shaderProgram);

	glEnableVertexAttribArray(GL4_ATTRIB_POSITION);
	// Note: the glVertexAttribPointer() configuration is stored in the VAO, not the shader or sth
	//       (that's why I use one VAO per 2D shader)
	qglVertexAttribPointer(GL4_ATTRIB_POSITION, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), 0);

	glEnableVertexAttribArray(GL4_ATTRIB_TEXCOORD);
	qglVertexAttribPointer(GL4_ATTRIB_TEXCOORD, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), 2*sizeof(float));

	// set up attribute layout for 2D flat color rendering

	glGenVertexArrays(1, &vao2Dcolor);
	glBindVertexArray(vao2Dcolor);

	GL4_BindVBO(vbo2D); // yes, both VAOs share the same VBO

	GL4_UseProgram(gl4state.si2Dcolor.shaderProgram);

	glEnableVertexAttribArray(GL4_ATTRIB_POSITION);
	qglVertexAttribPointer(GL4_ATTRIB_POSITION, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), 0);

	GL4_BindVAO(0);
}

void
GL4_Draw_ShutdownLocal(void)
{
	glDeleteBuffers(1, &vbo2D);
	vbo2D = 0;
	glDeleteVertexArrays(1, &vao2D);
	vao2D = 0;
	glDeleteVertexArrays(1, &vao2Dcolor);
	vao2Dcolor = 0;
}

// bind the texture before calling this
static void
drawTexturedRectangle(float x, float y, float w, float h,
                      float sl, float tl, float sh, float th)
{
	/*
	 *  x,y+h      x+w,y+h
	 * sl,th--------sh,th
	 *  |             |
	 *  |             |
	 *  |             |
	 * sl,tl--------sh,tl
	 *  x,y        x+w,y
	 */

	GLfloat vBuf[16] = {
	//  X,   Y,   S,  T
		x,   y+h, sl, th,
		x,   y,   sl, tl,
		x+w, y+h, sh, th,
		x+w, y,   sh, tl
	};

	GL4_BindVAO(vao2D);

	// Note: while vao2D "remembers" its vbo for drawing, binding the vao does *not*
	//       implicitly bind the vbo, so I need to explicitly bind it before glBufferData()
	GL4_BindVBO(vbo2D);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vBuf), vBuf, GL_STREAM_DRAW);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	//glMultiDrawArrays(mode, first, count, drawcount) ??
}

/*
 * Draws one 8*8 graphics character with 0 being transparent.
 * It can be clipped to the top of the screen to allow the console to be
 * smoothly scrolled off.
 */
void
GL4_Draw_CharScaled(int x, int y, int num, float scale)
{
	int row, col;
	float frow, fcol, size, scaledSize;
	num &= 255;

	if ((num & 127) == 32)
	{
		return; /* space */
	}

	if (y <= -8)
	{
		return; /* totally off screen */
	}

	row = num >> 4;
	col = num & 15;

	frow = row * 0.0625;
	fcol = col * 0.0625;
	size = 0.0625;

	scaledSize = 8*scale;

	// TODO: batchen?

	GL4_UseProgram(gl4state.si2D.shaderProgram);
	GL4_Bind(draw_chars->texnum);
	drawTexturedRectangle(x, y, scaledSize, scaledSize, fcol, frow, fcol+size, frow+size);
}

gl4image_t *
GL4_Draw_FindPic(char *name)
{
	return R_FindPic(name, (findimage_t)GL4_FindImage);
}

void
GL4_Draw_GetPicSize(int *w, int *h, char *pic)
{
	gl4image_t *gl;

	gl = R_FindPic(pic, (findimage_t)GL4_FindImage);

	if (!gl)
	{
		*w = *h = -1;
		return;
	}

	*w = gl->width;
	*h = gl->height;
}

void
GL4_Draw_StretchPic(int x, int y, int w, int h, char *pic)
{
	gl4image_t *gl = R_FindPic(pic, (findimage_t)GL4_FindImage);

	if (!gl)
	{
		R_Printf(PRINT_ALL, "Can't find pic: %s\n", pic);
		return;
	}

	GL4_UseProgram(gl4state.si2D.shaderProgram);
	GL4_Bind(gl->texnum);

	drawTexturedRectangle(x, y, w, h, gl->sl, gl->tl, gl->sh, gl->th);
}

void
GL4_Draw_PicScaled(int x, int y, char *pic, float factor)
{
	gl4image_t *gl = R_FindPic(pic, (findimage_t)GL4_FindImage);
	if (!gl)
	{
		R_Printf(PRINT_ALL, "Can't find pic: %s\n", pic);
		return;
	}

	GL4_UseProgram(gl4state.si2D.shaderProgram);
	GL4_Bind(gl->texnum);

	drawTexturedRectangle(x, y, gl->width*factor, gl->height*factor, gl->sl, gl->tl, gl->sh, gl->th);
}

void
GL4_Draw_PicScaledCol(int x, int y, const char *pic, float factor, const float color[3])
{
	gl4image_t *gl = R_FindPic(pic, (findimage_t)GL4_FindImage);
	if (!gl)
	{
		Com_Printf("Can't find pic: %s\n", pic);
		return;
	}

	gl4state.uniCommonData.color = HMM_Vec4(color[0], color[1], color[2], 1.0f);
	GL4_UpdateUBOCommon();

	GL4_UseProgram(gl4state.si2Dtinted.shaderProgram);
	GL4_Bind(gl->texnum);

	drawTexturedRectangle(x, y, gl->width*factor, gl->height*factor, gl->sl, gl->tl, gl->sh, gl->th);

	gl4state.uniCommonData.color = HMM_Vec4(1, 1, 1, 1);
	GL4_UpdateUBOCommon();
}

/*
 * This repeats a 64*64 tile graphic to fill
 * the screen around a sized down
 * refresh window.
 */
void
GL4_Draw_TileClear(int x, int y, int w, int h, char *pic)
{
	gl4image_t *image = R_FindPic(pic, (findimage_t)GL4_FindImage);
	if (!image)
	{
		R_Printf(PRINT_ALL, "Can't find pic: %s\n", pic);
		return;
	}

	GL4_UseProgram(gl4state.si2D.shaderProgram);
	GL4_Bind(image->texnum);

	drawTexturedRectangle(x, y, w, h, x/64.0f, y/64.0f, (x+w)/64.0f, (y+h)/64.0f);
}

void
GL4_DrawFrameBufferObject(int x, int y, int w, int h, GLuint fboTexture, const float v_blend[4])
{
    GLuint finalTex = fboTexture;
    qboolean bloomActive = false;

    /* check for r_bloom */
    if (r_bloom && r_bloom->value != 0.0f)
    {
        GLuint bloom = GL4_ApplyBloom(fboTexture, w, h);
        if (bloom != 0)
        {
            finalTex = bloom;
            bloomActive = true;
        }
    }

    qboolean underwater = (gl4_newrefdef.rdflags & RDF_UNDERWATER) != 0;
    gl4ShaderInfo_t* shader = underwater ? &gl4state.si2DpostProcessWater
                                         : &gl4state.si2DpostProcess;

    /* select shader and bind scene texture */
    GL4_UseProgram(shader->shaderProgram);
    GL4_Bind(finalTex);

    /* set shader uniforms if present */
    if (underwater && shader->uniLmScalesOrTime != -1)
    {
        glUniform1f(shader->uniLmScalesOrTime, gl4_newrefdef.time);
    }
    if (shader->uniVblend != -1)
    {
        glUniform4fv(shader->uniVblend, 1, v_blend);
    }

    /*
     * Build a small fullscreen quad vertex array and upload it into the
     * shared VBO. We use the same VBO/VAO used by other 2D draw helpers
     * (vao2D / vbo2D). The VAO already has pointers configured
     * in GL4_Draw_InitLocal(), so we only need to upload the vertex data.
     *
     * Vertex layout (matches VAO setup): X, Y, S, T
     */
    GLfloat fsQuad[16];

    if (bloomActive && underwater)
    {
        /* invert T coordinates to compensate for FBO orientation underwater */
        fsQuad[0]  = (GLfloat)x;       fsQuad[1]  = (GLfloat)(y + h); fsQuad[2]  = 0.0f; fsQuad[3]  = 0.0f;
        fsQuad[4]  = (GLfloat)x;       fsQuad[5]  = (GLfloat)y;       fsQuad[6]  = 0.0f; fsQuad[7]  = 1.0f;
        fsQuad[8]  = (GLfloat)(x + w); fsQuad[9]  = (GLfloat)(y + h); fsQuad[10] = 1.0f; fsQuad[11] = 0.0f;
        fsQuad[12] = (GLfloat)(x + w); fsQuad[13] = (GLfloat)y;       fsQuad[14] = 1.0f; fsQuad[15] = 1.0f;
    }
    else
    {
        fsQuad[0]  = (GLfloat)x;       fsQuad[1]  = (GLfloat)(y + h); fsQuad[2]  = 0.0f; fsQuad[3]  = 1.0f;
        fsQuad[4]  = (GLfloat)x;       fsQuad[5]  = (GLfloat)y;       fsQuad[6]  = 0.0f; fsQuad[7]  = 0.0f;
        fsQuad[8]  = (GLfloat)(x + w); fsQuad[9]  = (GLfloat)(y + h); fsQuad[10] = 1.0f; fsQuad[11] = 1.0f;
        fsQuad[12] = (GLfloat)(x + w); fsQuad[13] = (GLfloat)y;       fsQuad[14] = 1.0f; fsQuad[15] = 0.0f;
    }

    GL4_BindVAO(vao2D);
    GL4_BindVBO(vbo2D);

    glBufferData(GL_ARRAY_BUFFER, sizeof(fsQuad), fsQuad, GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    GL4_BindVAO(0);
    if (finalTex != fboTexture)
    {
        glDeleteTextures(1, &finalTex);
    }
}

/*
 * Fills a box of pixels with a single color
 */
void
GL4_Draw_Fill(int x, int y, int w, int h, int c)
{
	union
	{
		unsigned c;
		byte v[4];
	} color;
	int i;

	if ((unsigned)c > 255)
	{
		ri.Sys_Error(ERR_FATAL, "Draw_Fill: bad color");
	}

	color.c = d_8to24table[c];

	GLfloat vBuf[8] = {
	//  X,   Y
		x,   y+h,
		x,   y,
		x+w, y+h,
		x+w, y
	};

	for(i=0; i<3; ++i)
	{
		gl4state.uniCommonData.color.Elements[i] = color.v[i] * (1.0f/255.0f);
	}
	gl4state.uniCommonData.color.A = 1.0f;

	GL4_UpdateUBOCommon();

	GL4_UseProgram(gl4state.si2Dcolor.shaderProgram);
	GL4_BindVAO(vao2Dcolor);

	GL4_BindVBO(vbo2D);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vBuf), vBuf, GL_STREAM_DRAW);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

// in GL1 this is called R_Flash() (which just calls R_PolyBlend())
// now implemented in 2D mode and called after SetGL2D() because
// it's pretty similar to GL4_Draw_FadeScreen()
void
GL4_Draw_Flash(const float color[4], float x, float y, float w, float h)
{
	if (gl_polyblend->value == 0)
	{
		return;
	}

	int i=0;

	GLfloat vBuf[8] = {
	//  X,   Y
		x,   y+h,
		x,   y,
		x+w, y+h,
		x+w, y
	};

	glEnable(GL_BLEND);
    
	/* this blends the screen flash while bloom is enabled */
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	for(i=0; i<4; ++i)  gl4state.uniCommonData.color.Elements[i] = color[i];

	GL4_UpdateUBOCommon();

	GL4_UseProgram(gl4state.si2Dcolor.shaderProgram);

	GL4_BindVAO(vao2Dcolor);

	GL4_BindVBO(vbo2D);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vBuf), vBuf, GL_STREAM_DRAW);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glDisable(GL_BLEND);
}

void
GL4_Draw_FadeScreen(void)
{
	float color[4] = {0, 0, 0, 0.6f};
	GL4_Draw_Flash(color, 0, 0, vid.width, vid.height);
}

static
void GL4_PaletteExpandWorker(int row_start, int row_end, void* user)
{
    GL4_PalExpandCtx* ctx = (GL4_PalExpandCtx*)user;
    const int cols = ctx->cols;

    for (int i = row_start; i < row_end; ++i) {
        const int rowOffset = i * cols;
        const byte* __restrict src = &ctx->data[rowOffset];
        unsigned*   __restrict dst = &ctx->img[rowOffset];
        for (int j = 0; j < cols; ++j) {
            dst[j] = gl4_rawpalette[src[j]];
        }
    }
}

void
GL4_Draw_StretchRaw(int x, int y, int w, int h, int cols, int rows, const byte *data, int bits)
{
    GL4_Bind(0);

    unsigned image32[320*240];
    unsigned* img = image32;

    if (bits == 32) {
        img = (unsigned*)data;
    } else {
        const size_t pxCount = (size_t)cols * (size_t)rows;
        if (pxCount > (size_t)320 * (size_t)240) {
            img = (unsigned*)malloc(pxCount * sizeof(unsigned));
            if (!img) {
                uintptr_t need = (uintptr_t)(pxCount * sizeof(unsigned));
                R_Printf(PRINT_ALL, "GL4_Draw_StretchRaw: malloc(%" PRIuPTR ") failed\n", need);
                return;
            }
        }

        GL4_PalExpandCtx ctx = { data, img, cols };

        // atsb: parallel workers, ensure that work is done without idling
        GL4_ParallelTasks(rows, 32, GL4_PaletteExpandWorker, &ctx);
    }

    GL4_UseProgram(gl4state.si2D.shaderProgram);

    GLuint glTex;
    glGenTextures(1, &glTex);
    GL4_SelectTMU(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, glTex);

    glTexImage2D(GL_TEXTURE_2D, 0, gl4_tex_solid_format,
                 cols, rows, 0, GL_RGBA, GL_UNSIGNED_BYTE, img);

    if (img != image32 && img != (unsigned *)data) {
        free(img);
    }

    GLint filter = (r_videos_unfiltered->value == 0) ? gl_filter_max : GL_NEAREST;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);

    drawTexturedRectangle(x, y, w, h, 0.0f, 0.0f, 1.0f, 1.0f);

    glDeleteTextures(1, &glTex);

    GL4_Bind(0);
}

/* draw a fullscreen quad using the existing vao2D/vbo2D */
static void GL4_DrawFullscreenQuadFromArray(const GLfloat fsQuad[16])
{
    GL4_BindVAO(vao2D);
    GL4_BindVBO(vbo2D);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 16, fsQuad, GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    GL4_BindVAO(0);
}

/* Shutdown bloom resources */
void GL4_BloomShutdown(void)
{
    if (bloomFBO[0])
    {
        glDeleteFramebuffers(1, &bloomFBO[0]); bloomFBO[0] = 0;
    }
    if (bloomFBO[1])
    {
        glDeleteFramebuffers(1, &bloomFBO[1]); bloomFBO[1] = 0;
    }
    if (bloomTex[0])
    {
        glDeleteTextures(1, &bloomTex[0]); bloomTex[0] = 0;
    }
    if (bloomTex[1])
    {
        glDeleteTextures(1, &bloomTex[1]); bloomTex[1] = 0;
    }

    bloomInitialized = false;
}

/*
 * Apply a simple bloom effect
 *
 * Returns: GLuint of the composite bloom texture.
 * Caller must delete the returned texture when done.
 */
GLuint GL4_ApplyBloom(GLuint sceneTex, int sceneW, int sceneH)
{
    if (!r_bloom || r_bloom->value == 0.0f)
        return 0;

    int w = (sceneW > 0) ? sceneW : 1;
    int h = (sceneH > 0) ? sceneH : 1;

    int downscale = 2;
    int bw = (w / downscale) > 0 ? (w / downscale) : 1;
    int bh = (h / downscale) > 0 ? (h / downscale) : 1;

    /* create FBOs + textures */
    GLuint fboBright = 0, fboPing = 0, fboComp = 0;
    GLuint texBright = 0, texPing = 0, texComp = 0;

    glGenFramebuffers(1, &fboBright);
    glGenFramebuffers(1, &fboPing);
    glGenFramebuffers(1, &fboComp);

    glGenTextures(1, &texBright);
    glGenTextures(1, &texPing);
    glGenTextures(1, &texComp);

    GLenum internalFmt = gl4_tex_solid_format;
    GLenum fmt = GL_RGBA;
    GLenum type = GL_UNSIGNED_BYTE;

    /* texBright */
    GL4_SelectTMU(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texBright);
    glTexImage2D(GL_TEXTURE_2D, 0, internalFmt, bw, bh, 0, fmt, type, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    /* texPing */
    glBindTexture(GL_TEXTURE_2D, texPing);
    glTexImage2D(GL_TEXTURE_2D, 0, internalFmt, bw, bh, 0, fmt, type, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    /* texComp */
    glBindTexture(GL_TEXTURE_2D, texComp);
    glTexImage2D(GL_TEXTURE_2D, 0, internalFmt, w, h, 0, fmt, type, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    /* attach textures to FBOs */
    glBindFramebuffer(GL_FRAMEBUFFER, fboBright);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texBright, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, fboPing);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texPing, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, fboComp);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texComp, 0);

    /* check completeness */
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

    glBindFramebuffer(GL_FRAMEBUFFER, fboBright);
    glBindFramebuffer(GL_FRAMEBUFFER, fboPing);
    glBindFramebuffer(GL_FRAMEBUFFER, fboComp);
    
    if (status != GL_FRAMEBUFFER_COMPLETE)
        goto fail;

    /* save old viewport */
    glGetIntegerv(GL_VIEWPORT, oldViewPort);

    /* fullscreen quads for downsampled and full sizes (X,Y,S,T) */
    GLfloat fsQuadDown[16] = {
        -1.0f,  1.0f, 0.0f, 1.0f,
        -1.0f, -1.0f, 0.0f, 0.0f,
        1.0f,  1.0f, 1.0f, 1.0f,
        1.0f, -1.0f, 1.0f, 0.0f
    };
    GLfloat fsQuadFull[16] = {
        0.0f, (GLfloat)h, 0.0f, 1.0f,
        0.0f, 0.0f, 0.0f, 0.0f,
        (GLfloat)w, (GLfloat)h, 1.0f, 1.0f,
        (GLfloat)w, 0.0f, 1.0f, 0.0f
    };

    /* Bright pass */
    glBindFramebuffer(GL_FRAMEBUFFER, fboBright);
    glViewport(0, 0, bw, bh);
    glClear(GL_COLOR_BUFFER_BIT);

    if (gl4_bloomBright.shaderProgram)
    {
        GL4_UseProgram(gl4_bloomBright.shaderProgram);

        GL4_SelectTMU(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, sceneTex);
        GLint locTex = glGetUniformLocation(gl4_bloomBright.shaderProgram, "tex");
        if (locTex != -1) glUniform1i(locTex, 0);

        /* playable default value */
        float threshold = 0.75f;
        GLint locThreshold = glGetUniformLocation(gl4_bloomBright.shaderProgram, "threshold");
        if (locThreshold != -1) glUniform1f(locThreshold, threshold);

        GL4_DrawFullscreenQuadFromArray(fsQuadDown);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    /* blur pass */
    if (gl4_bloomBlur.shaderProgram)
    {
        GLint locTex = glGetUniformLocation(gl4_bloomBlur.shaderProgram, "tex");
        GLint locDir = glGetUniformLocation(gl4_bloomBlur.shaderProgram, "dir");

        glBindFramebuffer(GL_FRAMEBUFFER, fboPing);
        glViewport(0, 0, bw, bh);
        glClear(GL_COLOR_BUFFER_BIT);

        GL4_UseProgram(gl4_bloomBlur.shaderProgram);
        GL4_SelectTMU(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texBright);

        if (locTex != -1)
            glUniform1i(locTex, 0);

        if (locDir != -1)
            glUniform2f(locDir, 1.0f / (float)bw, 0.0f);

        GL4_DrawFullscreenQuadFromArray(fsQuadDown);
        glBindTexture(GL_TEXTURE_2D, 0);

        /* vertical blur */
        glBindFramebuffer(GL_FRAMEBUFFER, fboBright);
        glViewport(0, 0, bw, bh);
        glClear(GL_COLOR_BUFFER_BIT);

        GL4_UseProgram(gl4_bloomBlur.shaderProgram);
        GL4_SelectTMU(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texPing);

        if (locTex != -1)
            glUniform1i(locTex, 0);
        if (locDir != -1)
            glUniform2f(locDir, 0.0f, 1.0f / (float)bh);

        GL4_DrawFullscreenQuadFromArray(fsQuadDown);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    /* composite pass*/
    glBindFramebuffer(GL_FRAMEBUFFER, fboComp);
    glViewport(0, 0, w, h);
    glClear(GL_COLOR_BUFFER_BIT);

    /* render base scene */
    glDisable(GL_BLEND);
    GL4_UseProgram(gl4state.si2D.shaderProgram);
    GL4_SelectTMU(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneTex);
    GL4_DrawFullscreenQuadFromArray(fsQuadFull);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);

    GL4_UpdateUBOCommon();

    GL4_UseProgram(gl4state.si2Dtinted.shaderProgram);
    GL4_SelectTMU(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texBright);
    GL4_DrawFullscreenQuadFromArray(fsQuadFull);

    /* restore engine blending */
    glDisable(GL_BLEND);
    gl4state.uniCommonData.color = HMM_Vec4(1.0f, 1.0f, 1.0f, 1.0f);
    GL4_UpdateUBOCommon();

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(oldViewPort[0], oldViewPort[1], oldViewPort[2], oldViewPort[3]);

    /* cleanup */
    glDeleteFramebuffers(1, &fboBright);
    glDeleteFramebuffers(1, &fboPing);
    glDeleteFramebuffers(1, &fboComp);

    glDeleteTextures(1, &texPing);
    glDeleteTextures(1, &texBright);

    return texComp;

fail:
    if (fboBright)
        glDeleteFramebuffers(1, &fboBright);
    if (fboPing)
        glDeleteFramebuffers(1, &fboPing);
    if (fboComp)
        glDeleteFramebuffers(1, &fboComp);
    if (texBright)
        glDeleteTextures(1, &texBright);
    if (texPing)
        glDeleteTextures(1, &texPing);
    if (texComp)
        glDeleteTextures(1, &texComp);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return 0;
}
