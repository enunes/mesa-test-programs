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
	assert(fread(buff, 1, size, f) == size);
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

	glUseProgram(program);
}

void Render(void)
{
   GLfloat fu0[4] = { 0.007212 , 0.001496 , 0.005935 , 0.004095 };
   GLfloat fu1[4] = { 0.001449 , 0.001881 , 0.003066 , 0.001799 };
   GLfloat fu2[4] = { 0.001318 , 0.009491 , 0.001249 , 0.002285 };
   GLfloat fu3[4] = { 0.001522 , 0.001391 , 0.001383 , 0.002047 };
   GLfloat fu4[4] = { 0.001550 , 0.001940 , 0.002526 , 0.001737 };
   GLfloat fu5[4] = { 0.007111 , 0.002221 , 0.006506 , 0.001327 };
   GLfloat fu6[4] = { 0.004827 , 0.009871 , 0.001033 , 0.007798 };
   GLfloat fu7[4] = { 0.002773 , 0.001458 , 0.002801 , 0.001134 };
   GLfloat fu8[4] = { 0.001414 , 0.003156 , 0.001467 , 0.002321 };
   GLfloat fu9[4] = { 0.001001 , 0.006786 , 0.001329 , 0.003189 };

   GLint u0 = glGetUniformLocation(program, "u0");
   GLint u1 = glGetUniformLocation(program, "u1");
   GLint u2 = glGetUniformLocation(program, "u2");
   GLint u3 = glGetUniformLocation(program, "u3");
   GLint u4 = glGetUniformLocation(program, "u4");
   GLint u5 = glGetUniformLocation(program, "u5");
   GLint u6 = glGetUniformLocation(program, "u6");
   GLint u7 = glGetUniformLocation(program, "u7");
   GLint u8 = glGetUniformLocation(program, "u8");
   GLint u9 = glGetUniformLocation(program, "u9");
   glUniform4fv(u0, 1, fu0);
   glUniform4fv(u1, 1, fu1);
   glUniform4fv(u2, 1, fu2);
   glUniform4fv(u3, 1, fu3);
   glUniform4fv(u4, 1, fu4);
   glUniform4fv(u5, 1, fu5);
   glUniform4fv(u6, 1, fu6);
   glUniform4fv(u7, 1, fu7);
   glUniform4fv(u8, 1, fu8);
   glUniform4fv(u9, 1, fu9);

	GLfloat vertex[] = {
		-1, -1, 0,
		-1, 1, 0,
		1, 1, 0,
		1, -1, 0
	};
	GLfloat color[] = {
		1, 0, 0, 1,
		0, 1, 0, 1,
		0, 0, 1, 1,
	};
	//GLuint index[] = {
	//	0, 1, 2
	//};

	GLint position = glGetAttribLocation(program, "positionIn");
	glEnableVertexAttribArray(position);
	glVertexAttribPointer(position, 3, GL_FLOAT, 0, 0, vertex);

	GLint colorIn = glGetAttribLocation(program, "colorIn");
	glEnableVertexAttribArray(colorIn);
	glVertexAttribPointer(colorIn, 4, GL_FLOAT, 0, 0, color);

	assert(glGetError() == GL_NO_ERROR);

	glClear(GL_COLOR_BUFFER_BIT
			//| GL_DEPTH_BUFFER_BIT
	       );
	printf("%x\n", glGetError());
	assert(glGetError() == GL_NO_ERROR);

	//glDrawElements(GL_TRIANGLES, sizeof(index)/sizeof(GLuint), GL_UNSIGNED_INT, index);
	glDrawArrays(GL_TRIANGLES, 0, 3);
   glEnable(GL_SCISSOR_TEST);
   glScissor(50, 50, 100, 100);
   glClear(GL_COLOR_BUFFER_BIT);


	assert(glGetError() == GL_NO_ERROR);

	eglSwapBuffers(display, surface);
}
