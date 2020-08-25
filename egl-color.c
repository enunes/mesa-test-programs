#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>

GLuint program;
EGLDisplay display;
EGLSurface surface;

static GLuint opengl_vbo;

static GLuint LoadShader(const char *name, GLenum type)
{
	FILE *f;
	int size;
	char *buff;
	GLuint shader;
	GLint compiled;
	const GLchar *source[1];

	assert((f = fopen(name, "r")) != NULL);

	// get file size
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fseek(f, 0, SEEK_SET);

	assert((buff = malloc(size)) != NULL);
	assert((int)fread(buff, 1, size, f) == (int)size);
	source[0] = buff;
	fclose(f);
	shader = glCreateShader(type);
	glShaderSource(shader, 1, source, &size);
	glCompileShader(shader);
	free(buff);
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
	if (!compiled) {
		GLint infoLen = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
		if (infoLen > 1) {
			char *infoLog = malloc(infoLen);
			glGetShaderInfoLog(shader, infoLen, NULL, infoLog);
			fprintf(stderr, "Error compiling shader %s:\n%s\n", name, infoLog);
			free(infoLog);
		}
		glDeleteShader(shader);
		return 0;
	}

	return shader;
}

void InitGLES(int width, int height)
{
	GLint linked;
	GLuint vertexShader;
	GLuint fragmentShader;
	assert((vertexShader = LoadShader("egl-color.vert", GL_VERTEX_SHADER)) != 0);
	assert((fragmentShader = LoadShader("egl-color.frag", GL_FRAGMENT_SHADER)) != 0);
	assert((program = glCreateProgram()) != 0);
	glAttachShader(program, vertexShader);
	glAttachShader(program, fragmentShader);
	glLinkProgram(program);
	glGetProgramiv(program, GL_LINK_STATUS, &linked);
	if (!linked) {
		GLint infoLen = 0;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLen);
		if (infoLen > 1) {
			char *infoLog = malloc(infoLen);
			glGetProgramInfoLog(program, infoLen, NULL, infoLog);
			fprintf(stderr, "Error linking program:\n%s\n", infoLog);
			free(infoLog);
		}
		glDeleteProgram(program);
		exit(1);
	}

	glClearColor(0, 0, 0, 0);
	glViewport(0, 0, width, height);
	//glEnable(GL_DEPTH_TEST);

	glGenBuffers(1, &opengl_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, opengl_vbo);

	glUseProgram(program);
}

void Render(void)
{
	GLfloat buf_vbo[] = {
		-0.99, -0.99, 0,             0.99, 0, 0, 0.99,
		-0.99,  0.99, 0,             0, 0.99, 0, 0.99,
		 0.99,  0.99, 0,             0, 0, 0.99, 0.99,
	};

	GLfloat buf_vbo2[] = {
		-1, -1, 0,             1, 0, 0, 1,
		 1, -1, 0,             0, 1, 1, 1,
		 1,  1, 0,             0, 0, 1, 1,

// uncomment to fix issue (different size buffers)
//		-1, -1, 0,             1, 0, 0, 1,
//		 1, -1, 0,             0, 1, 1, 1,
//		 1,  1, 0,             0, 0, 1, 1,
	};

	GLint position = glGetAttribLocation(program, "positionIn");
	glEnableVertexAttribArray(position);
	glVertexAttribPointer(position, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void *) (0 * sizeof(float)));

	GLint colorIn = glGetAttribLocation(program, "colorIn");
	glEnableVertexAttribArray(colorIn);
	glVertexAttribPointer(colorIn, 4, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void *) (3 * sizeof(float)));

	assert(glGetError() == GL_NO_ERROR);

	glClear(GL_COLOR_BUFFER_BIT
			//| GL_DEPTH_BUFFER_BIT
	       );
	printf("%x\n", glGetError());
	assert(glGetError() == GL_NO_ERROR);

	//glDrawElements(GL_TRIANGLES, sizeof(index)/sizeof(GLuint), GL_UNSIGNED_INT, index);
	glBufferData(GL_ARRAY_BUFFER, sizeof(buf_vbo), buf_vbo, GL_STREAM_DRAW);

	glDrawArrays(GL_TRIANGLES, 0, 3);

	// uncomment to fix issue (force new buffer generation)
	//   glGenBuffers(1, &opengl_vbo);
	//   glBindBuffer(GL_ARRAY_BUFFER, opengl_vbo);

	glBufferData(GL_ARRAY_BUFFER, sizeof(buf_vbo2), buf_vbo2, GL_STREAM_DRAW);
	glDrawArrays(GL_TRIANGLES, 0, 3);

	assert(glGetError() == GL_NO_ERROR);

	eglSwapBuffers(display, surface);
}
