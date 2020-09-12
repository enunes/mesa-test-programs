attribute vec3 positionIn;
attribute vec4 colorIn;

varying vec4 color;

uniform vec4 u0;
uniform vec4 u1;
uniform vec4 u2;
uniform vec4 u3;
uniform vec4 u4;
uniform vec4 u5;
uniform vec4 u6;
uniform vec4 u7;
uniform vec4 u8;
uniform vec4 u9;

void main()
{
    color = colorIn + u0 + u1 + u2 + u3 + u4 + u5 + u6 + u7 + u8 + u9;
    gl_Position = vec4(positionIn, 1);
}
