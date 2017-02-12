#include "glad/glad.h"
#include "glfw/glfw_config.h"
#include "glfw/glfw3.h"

#define TINYGL_IMPL
#include "../tinygl.h"

#define TT_IMPLEMENTATION
#include "../tinytime.h"

#define TINYC2_IMPL
#include "../tinyc2.h"

GLFWwindow* window;
float projection[ 16 ];
tgShader simple;
int use_post_fx = 0;
tgFramebuffer fb;
tgShader post_fx;
int spaced_pressed;
void* ctx;
float screen_w;
float screen_h;
c2v mp;
float wheel;

c2Circle user_circle;
c2Capsule user_capsule;
float user_rotation;

void* ReadFileToMemory( const char* path, int* size )
{
	void* data = 0;
	FILE* fp = fopen( path, "rb" );
	int sizeNum = 0;

	if ( fp )
	{
		fseek( fp, 0, SEEK_END );
		sizeNum = ftell( fp );
		fseek( fp, 0, SEEK_SET );
		data = malloc( sizeNum + 1 );
		fread( data, sizeNum, 1, fp );
		((char*)data)[ sizeNum ] = 0;
		fclose( fp );
	}

	if ( size ) *size = sizeNum;
	return data;
}

void ErrorCB( int error, const char* description )
{
	fprintf( stderr, "Error: %s\n", description );
}

void KeyCB( GLFWwindow* window, int key, int scancode, int action, int mods )
{
	if ( key == GLFW_KEY_ESCAPE && action == GLFW_PRESS )
		glfwSetWindowShouldClose( window, GLFW_TRUE );

	if ( key == GLFW_KEY_SPACE && action == GLFW_PRESS )
		spaced_pressed = 1;

	if ( key == GLFW_KEY_SPACE && action == GLFW_RELEASE )
		spaced_pressed = 0;

	if ( key == GLFW_KEY_P && action == GLFW_PRESS )
		use_post_fx = !use_post_fx;
}

void ScrollCB( GLFWwindow* window, double x, double y )
{
	(void)x;
	wheel = (float)y;
}

void Rotate( c2v* src, c2v* dst, int count )
{
	if ( !wheel ) return;
	c2r r = c2Rot( wheel > 0 ? 3.14159265f / 16.0f : -3.14159265f / 16.0f );
	for ( int i = 0; i < count; ++i ) dst[ i ] = c2Mulrv( r, src[ i ] );
}

c2Capsule GetCapsule( )
{
	c2Capsule cap = user_capsule;
	cap.a = c2Add( mp, cap.a );
	cap.b = c2Add( mp, cap.b );
	return cap;
}

void MouseCB( GLFWwindow* window, double x, double y )
{
	float mouse_x = (float)x - screen_w / 2;
	float mouse_y = -((float)y - screen_h / 2);
	mp = c2V( mouse_x, mouse_y );

	user_circle.p = mp;
	user_circle.r = 10.0f;
}

void ResizeFramebuffer( int w, int h )
{
	static int first = 1;
	if ( first ) 
	{
		first = 0;
		char* vs = (char*)ReadFileToMemory( "postprocess.vs", 0 );
		char* ps = (char*)ReadFileToMemory( "postprocess.ps", 0 );
		tgLoadShader( &post_fx, vs, ps );
		free( vs );
		free( ps );
	}
	else tgFreeFramebuffer( &fb );
	screen_w = (float)w;
	screen_h = (float)h;
	tgMakeFramebuffer( &fb, &post_fx, w, h );
}

void Reshape( GLFWwindow* window, int width, int height )
{
	tgOrtho2D( (float)width, (float)height, 0, 0, projection );
	glViewport( 0, 0, width, height );
	ResizeFramebuffer( width, height );
}

void SwapBuffers( )
{
	glfwSwapBuffers( window );
}

// enable depth test here if you care, also clear
void GLSettings( )
{
}

#include <vector>

struct Color
{
	float r;
	float g;
	float b;
};

struct Vertex
{
	c2v pos;
	Color col;
};

std::vector<Vertex> verts;

void DrawPoly( c2v* verts, int count )
{
	for ( int i = 0; i < count; ++i )
	{
		int iA = i;
		int iB = (i + 1) % count;
		c2v a = verts[ iA ];
		c2v b = verts[ iB ];
		tgLine( ctx, a.x, a.y, 0, b.x, b.y, 0 );
	}
}

void DrawAABB( c2v a, c2v b )
{
	c2v c = c2V( a.x, b.y );
	c2v d = c2V( b.x, a.y );
	tgLine( ctx, a.x, a.y, 0, c.x, c.y, 0 );
	tgLine( ctx, c.x, c.y, 0, b.x, b.y, 0 );
	tgLine( ctx, b.x, b.y, 0, d.x, d.y, 0 );
	tgLine( ctx, d.x, d.y, 0, a.x, a.y, 0 );
}

void DrawHalfCircle( c2v a, c2v b )
{
	c2v u = c2Sub( b, a );
	float r = c2Len( u );
	u = c2Skew( u );
	c2v v = c2CW90( u );
	c2v s = c2Add( v, a );
	c2m m;
	m.x = c2Norm( u );
	m.y = c2Norm( v );

	int kSegs = 20;
	float theta = 0;
	float inc = 3.14159265f / (float)kSegs;
	c2v p0;
	c2SinCos( theta, &p0.y, &p0.x );
	p0 = c2Mulvs( p0, r );
	p0 = c2Add( c2Mulmv( m, p0 ), a );
	for ( int i = 0; i < kSegs; ++i )
	{
		theta += inc;
		c2v p1;
		c2SinCos( theta, &p1.y, &p1.x );
		p1 = c2Mulvs( p1, r );
		p1 = c2Add( c2Mulmv( m, p1 ), a );
		tgLine( ctx, p0.x, p0.y, 0, p1.x, p1.y, 0 );
		p0 = p1;
	}
}

void DrawCapsule( c2v a, c2v b, float r )
{
	c2v n = c2Norm( c2Sub( b, a ) );
	DrawHalfCircle( a, c2Add( a, c2Mulvs( n, -r ) ) );
	DrawHalfCircle( b, c2Add( b, c2Mulvs( n, r ) ) );
	c2v p0 = c2Add( a, c2Mulvs( c2Skew( n ), r ) );
	c2v p1 = c2Add( b, c2Mulvs( c2CW90( n ), -r ) );
	tgLine( ctx, p0.x, p0.y, 0, p1.x, p1.y, 0 );
	p0 = c2Add( a, c2Mulvs( c2Skew( n ), -r ) );
	p1 = c2Add( b, c2Mulvs( c2CW90( n ), r ) );
	tgLine( ctx, p0.x, p0.y, 0, p1.x, p1.y, 0 );
}

void DrawCircle( c2v p, float r )
{
	int kSegs = 40;
	float theta = 0;
	float inc = 3.14159265f * 2.0f / (float)kSegs;
	float px, py;
	c2SinCos( theta, &py, &px );
	px *= r; py *= r;
	px += p.x; py += p.y;
	for ( int i = 0; i <= kSegs; ++i )
	{
		theta += inc;
		float x, y;
		c2SinCos( theta, &y, &x );
		x *= r; y *= r;
		x += p.x; y += p.y;
		tgLine( ctx, x, y, 0, px, py, 0 );
		px = x; py = y;
	}
}

// should see slow rotation CCW, then CW
// space toggles between two different rotation implements
// after toggling implementations space toggles rotation direction
void TestRotation( )
{
	static int first = 1;
	static Vertex v[ 3 ];
	if ( first )
	{
		first = 0;
		Color c = { 1.0f, 0.0f, 0.0f };
		v[ 0 ].col = c;
		v[ 1 ].col = c;
		v[ 2 ].col = c;
		v[ 0 ].pos = c2V( 0, 100 );
		v[ 1 ].pos = c2V( 0, 0 );
		v[ 2 ].pos = c2V( 100, 0 );
	}

	static int which0;
	static int which1;
	if ( spaced_pressed ) which0 = !which0;
	if ( spaced_pressed && which0 ) which1 = !which1;

	if ( which0 )
	{
		c2m m;
		m.x = c2Norm( c2V( 1, 0.01f ) );
		m.y = c2Skew( m.x );
		for ( int i = 0; i < 3; ++i )
			v[ i ].pos = which1 ? c2Mulmv( m, v[ i ].pos ) : c2MulmvT( m, v[ i ].pos );
	}

	else
	{
		c2r r = c2Rot( 0.01f );
		for ( int i = 0; i < 3; ++i )
			v[ i ].pos = which1 ? c2Mulrv( r, v[ i ].pos ) : c2MulrvT( r, v[ i ].pos );
	}

	for ( int i = 0; i < 3; ++i )
		verts.push_back( v[ i ] );
}

void TestDrawPrim( )
{
	TestRotation( );

	tgLineColor( ctx, 0.2f, 0.6f, 0.8f );
	tgLine( ctx, 0, 0, 0, 100, 100, 0 );
	tgLineColor( ctx, 0.8f, 0.6f, 0.2f );
	tgLine( ctx, 100, 100, 0, -100, 200, 0 );

	DrawCircle( c2V( 0, 0 ), 100.0f );

	tgLineColor( ctx, 0, 1.0f, 0 );
	DrawHalfCircle( c2V( 0, 0 ), c2V( 50, -50 ) );

	tgLineColor( ctx, 0, 0, 1.0f );
	DrawCapsule( c2V( 0, 200 ), c2V( 75, 150 ), 20.0f );

	tgLineColor( ctx, 1.0f, 0, 0 );
	DrawAABB( c2V( -20, -20 ), c2V( 20, 20 ) );

	tgLineColor( ctx, 0.5f, 0.9f, 0.1f );
	c2v poly[] = {
		{ 0, 0 },
		{ 20.0f, 10.0f },
		{ 5.0f, 15.0f },
		{ -3.0f, 7.0f },
	};
	DrawPoly( poly, 4 );
}

void TestBoolean0( )
{
	c2AABB aabb;
	aabb.min = c2V( -40.0f, -40.0f );
	aabb.max = c2V( -15.0f, -15.0f );

	c2Circle circle;
	circle.p = c2V( -70.0f, 0 );
	circle.r = 20.0f;

	c2Capsule capsule;
	capsule.a = c2V( -40.0f, 40.0f );
	capsule.b = c2V( -20.0f, 100.0f );
	capsule.r = 10.0f;

	if ( c2CircletoCircle( user_circle, circle ) ) tgLineColor( ctx, 1.0f, 0.0f, 0.0f );
	else tgLineColor( ctx, 5.0f, 7.0f, 9.0f );
	DrawCircle( circle.p, circle.r );

	if ( c2CircletoAABB( user_circle, aabb ) ) tgLineColor( ctx, 1.0f, 0.0f, 0.0f );
	else tgLineColor( ctx, 5.0f, 7.0f, 9.0f );
	DrawAABB( aabb.min, aabb.max );

	if ( c2CircletoCapsule( user_circle, capsule ) ) tgLineColor( ctx, 1.0f, 0.0f, 0.0f );
	else tgLineColor( ctx, 5.0f, 7.0f, 9.0f );
	DrawCapsule( capsule.a, capsule.b, capsule.r );

	tgLineColor( ctx, 0.5f, 0.7f, 0.9f );
	DrawCircle( user_circle.p, user_circle.r );
}

void TestBoolean1( )
{
	c2AABB bb;
	bb.min = c2V( -100.0f, -30.0f );
	bb.max = c2V( -50.0f, 30.0f );
	c2Capsule cap = GetCapsule( );

	c2v a, b;
	c2GJK( &bb, C2_AABB, 0, &cap, C2_CAPSULE, 0, &a, &b, 1 );
	DrawCircle( a, 2.0f );
	DrawCircle( b, 2.0f );
	tgLine( ctx, a.x, a.y, 0, b.x, b.y, 0 );

	if ( c2AABBtoCapsule( bb, cap ) ) tgLineColor( ctx, 1.0f, 0.0f, 0.0f );
	else tgLineColor( ctx, 5.0f, 7.0f, 9.0f );
	DrawAABB( bb.min, bb.max );

	tgLineColor( ctx, 0.5f, 0.7f, 0.9f );
	DrawCapsule( cap.a, cap.b, cap.r );
}

float randf( )
{
	float r = (float)(rand( ) & RAND_MAX);
	r /= RAND_MAX;
	r = 2.0f * r - 1.0f;
	return r;
}

c2v RandomVec( )
{
	return c2V( randf( ) * 100.0f, randf( ) * 100.0f );
}

void TestBoolean2( )
{
	static c2Poly poly;
	static c2Poly poly2;
	static int first = 1;
	if ( first )
	{
		first = 0;
		poly.count = C2_MAX_POLYGON_VERTS;
		for ( int i = 0; i < poly.count; ++i ) poly.verts[ i ] = RandomVec( );
		poly.count = c2Hull( poly.verts, poly.count );
		poly2.count = C2_MAX_POLYGON_VERTS;
		for ( int i = 0; i < poly2.count; ++i ) poly2.verts[ i ] = RandomVec( );
		poly2.count = c2Hull( poly2.verts, poly2.count );
	}

	static int which;
	if ( spaced_pressed ) which = (which + 1) % 4;
	if ( wheel ) Rotate( poly2.verts, poly2.verts, poly2.count );

	switch ( which )
	{
	case 0:
	{
		c2v a, b;
		c2GJK( &user_circle, C2_CIRCLE, 0, &poly, C2_POLY, 0, &a, &b, 1 );
		DrawCircle( a, 2.0f );
		DrawCircle( b, 2.0f );
		tgLine( ctx, a.x, a.y, 0, b.x, b.y, 0 );

		if ( c2CircletoPoly( user_circle, &poly, 0 ) ) tgLineColor( ctx, 1.0f, 0.0f, 0.0f );
		else tgLineColor( ctx, 5.0f, 7.0f, 9.0f );
		DrawPoly( poly.verts, poly.count );

		tgLineColor( ctx, 0.5f, 0.7f, 0.9f );
		DrawCircle( user_circle.p, user_circle.r );
	}	break;

	case 1:
	{
		c2v a, b;
		c2AABB bb;
		bb.min = c2V( -10.0f, -10.0f );
		bb.max = c2V( 10.0f, 10.0f );
		bb.min = c2Add( bb.min, mp );
		bb.max = c2Add( bb.max, mp );
		c2GJK( &bb, C2_AABB, 0, &poly, C2_POLY, 0, &a, &b, 1 );
		DrawCircle( a, 2.0f );
		DrawCircle( b, 2.0f );
		tgLine( ctx, a.x, a.y, 0, b.x, b.y, 0 );

		if ( c2AABBtoPoly( bb, &poly, 0 ) ) tgLineColor( ctx, 1.0f, 0.0f, 0.0f );
		else tgLineColor( ctx, 5.0f, 7.0f, 9.0f );
		DrawPoly( poly.verts, poly.count );

		tgLineColor( ctx, 0.5f, 0.7f, 0.9f );
		DrawAABB( bb.min, bb.max );
	}	break;

	case 2:
	{
		c2v a, b;
		c2Capsule cap = GetCapsule( );
		c2GJK( &cap, C2_CAPSULE, 0, &poly, C2_POLY, 0, &a, &b, 1 );
		DrawCircle( a, 2.0f );
		DrawCircle( b, 2.0f );
		tgLine( ctx, a.x, a.y, 0, b.x, b.y, 0 );

		if ( c2CapsuletoPoly( cap, &poly, 0 ) ) tgLineColor( ctx, 1.0f, 0.0f, 0.0f );
		else tgLineColor( ctx, 5.0f, 7.0f, 9.0f );
		DrawPoly( poly.verts, poly.count );

		tgLineColor( ctx, 0.5f, 0.7f, 0.9f );
		DrawCapsule( cap.a, cap.b, cap.r );
	}	break;

	case 3:
	{
		c2v a, b;
		c2Poly poly3;
		for ( int i = 0; i < poly2.count; ++i ) poly3.verts[ i ] = c2Add( mp, poly2.verts[ i ] );
		poly3.count = poly2.count;

		c2GJK( &poly, C2_POLY, 0, &poly3, C2_POLY, 0, &a, &b, 1 );
		DrawCircle( a, 2.0f );
		DrawCircle( b, 2.0f );
		tgLine( ctx, a.x, a.y, 0, b.x, b.y, 0 );

		if ( c2PolytoPoly( &poly, 0, &poly3, 0 ) ) tgLineColor( ctx, 1.0f, 0.0f, 0.0f );
		else tgLineColor( ctx, 5.0f, 7.0f, 9.0f );
		DrawPoly( poly.verts, poly.count );

		tgLineColor( ctx, 0.5f, 0.7f, 0.9f );
		DrawPoly( poly3.verts, poly3.count );
	}	break;
	}
}

void TestRay0( )
{
	c2Circle c;
	c.p = c2V( 0, 0 );
	c.r = 20.0f;

	c2AABB bb;
	bb.min = c2V( 30.0f, 30.0f );
	bb.max = c2V( 70.0f, 70.0f );

	c2Ray ray;
	ray.p = c2V( -100.0f, 100.0f );
	ray.d = c2Norm( c2Sub( mp, ray.p ) );
	ray.t = c2Dot( mp, ray.d ) - c2Dot( ray.p, ray.d );

	tgLineColor( ctx, 1.0f, 1.0f, 1.0f );
	DrawCircle( c.p, c.r );
	DrawAABB( bb.min, bb.max );

	c2Raycast cast;
	int hit = c2RaytoCircle( ray, c, &cast );
	if ( hit )
	{
		ray.t = cast.t;
		c2v impact = c2Impact( ray, ray.t );
		c2v end = c2Add( impact, c2Mulvs( cast.n, 15.0f ) );
		tgLineColor( ctx, 1.0f, 0.2f, 0.4f );
		tgLine( ctx, impact.x, impact.y, 0, end.x, end.y, 0 );
		tgLine( ctx, ray.p.x, ray.p.y, 0, ray.p.x + ray.d.x * ray.t, ray.p.y + ray.d.y * ray.t, 0 );
	}

	else
	{

		ray.d = c2Norm( c2Sub( mp, ray.p ) );
		ray.t = c2Dot( mp, ray.d ) - c2Dot( ray.p, ray.d );

		if ( c2RaytoAABB( ray, bb, &cast ) )
		{
			ray.t = cast.t;
			c2v impact = c2Impact( ray, ray.t );
			c2v end = c2Add( impact, c2Mulvs( cast.n, 15.0f ) );
			tgLineColor( ctx, 1.0f, 0.2f, 0.4f );
			tgLine( ctx, impact.x, impact.y, 0, end.x, end.y, 0 );
		}
		else tgLineColor( ctx, 1.0f, 1.0f, 1.0f );

		tgLine( ctx, ray.p.x, ray.p.y, 0, ray.p.x + ray.d.x * ray.t, ray.p.y + ray.d.y * ray.t, 0 );
	}
}

void TestRay1( )
{
	c2Capsule cap;
	cap.a = c2V( -100.0f, 60.0f );
	cap.b = c2V( 50.0f, -40.0f );
	cap.r = 20.0f;

	c2Ray ray;
	ray.p = c2V( 75.0f, 100.0f );
	ray.d = c2Norm( c2Sub( mp, ray.p ) );
	ray.t = c2Dot( mp, ray.d ) - c2Dot( ray.p, ray.d );

	tgLineColor( ctx, 1.0f, 1.0f, 1.0f );
	DrawCapsule( cap.a, cap.b, cap.r );
	c2Raycast cast;
	if ( c2RaytoCapsule( ray, cap, &cast ) )
	{
		ray.t = cast.t;
		c2v impact = c2Impact( ray, ray.t );
		c2v end = c2Add( impact, c2Mulvs( cast.n, 15.0f ) );
		tgLineColor( ctx, 1.0f, 0.2f, 0.4f );
		tgLine( ctx, impact.x, impact.y, 0, end.x, end.y, 0 );
	}

	tgLine( ctx, ray.p.x, ray.p.y, 0, ray.p.x + ray.d.x * ray.t, ray.p.y + ray.d.y * ray.t, 0 );
}

int main( )
{
	// glfw and glad setup
	glfwSetErrorCallback( ErrorCB );

	if ( !glfwInit( ) )
		return 1;

	glfwWindowHint( GLFW_CONTEXT_VERSION_MAJOR, 3 );
	glfwWindowHint( GLFW_CONTEXT_VERSION_MINOR, 2 );
	glfwWindowHint( GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE );
	glfwWindowHint( GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE );

	int width = 640;
	int height = 480;
	window = glfwCreateWindow( width, height, "tinyc2 and tinygl", NULL, NULL );

	if ( !window )
	{
		glfwTerminate( );
		return 1;
	}

	glfwSetScrollCallback( window, ScrollCB );
	glfwSetCursorPosCallback( window, MouseCB );
	glfwSetKeyCallback( window, KeyCB );
	glfwSetFramebufferSizeCallback( window, Reshape );

	glfwMakeContextCurrent( window );
	gladLoadGLLoader( (GLADloadproc)glfwGetProcAddress );
	glfwSwapInterval( 1 );

	glfwGetFramebufferSize( window, &width, &height );
	Reshape( window, width, height );

	// tinygl setup
	// the clear bits are used in glClear, the settings bits are used in glEnable
	// use the | operator to mask together settings/bits, example settings_bits: GL_DEPTH_TEST | GL_STENCIL_TEST
	// either can be 0
	int max_draw_calls_per_flush = 32;
	int clear_bits = GL_COLOR_BUFFER_BIT;
	int settings_bits = 0;
	ctx = tgMakeCtx( max_draw_calls_per_flush, clear_bits, settings_bits );

	// define the attributes of vertices, which are inputs to the vertex shader
	// only accepts GL_TRIANGLES in 4th parameter
	// 5th parameter can be GL_STATIC_DRAW or GL_DYNAMIC_DRAW, which controls triple buffer or single buffering
	tgVertexData vd;
	tgMakeVertexData( &vd, 1024 * 1024, GL_TRIANGLES, sizeof( Vertex ), GL_DYNAMIC_DRAW );
	tgAddAttribute( &vd, "in_pos", 2, TG_FLOAT, TG_OFFSET_OF( Vertex, pos ) );
	tgAddAttribute( &vd, "in_col", 3, TG_FLOAT, TG_OFFSET_OF( Vertex, col ) );

	// a renderable holds a shader (tgShader) + vertex definition (tgVertexData)
	// renderables are used to construct draw calls (see main loop below)
	tgRenderable r;
	tgMakeRenderable( &r, &vd );
	char* vs = (char*)ReadFileToMemory( "simple.vs", 0 );
	char* ps = (char*)ReadFileToMemory( "simple.ps", 0 );
	TG_ASSERT( vs );
	TG_ASSERT( ps );
	tgLoadShader( &simple, vs, ps );
	free( vs );
	free( ps );
	tgSetShader( &r, &simple );
	tgSendMatrix( &simple, "u_mvp", projection );
	tgLineMVP( ctx, projection );

	// setup some models
	user_capsule.a = c2V( -30.0f, 0 );
	user_capsule.b = c2V( 30.0f, 0 );
	user_capsule.r = 10.0f;

	// main loop
	glClearColor( 0, 0, 0, 1 );
	float t = 0;
	while ( !glfwWindowShouldClose( window ) )
	{
		if ( spaced_pressed == 1 ) spaced_pressed = 0;
		wheel = 0;
		glfwPollEvents( );

		float dt = ttTime( );
		t += dt;
		t = fmod( t, 2.0f * 3.14159265f );
		tgSendF32( &post_fx, "u_time", 1, &t, 1 );

		if ( wheel ) Rotate( (c2v*)&user_capsule, (c2v*)&user_capsule, 2 );

		//TestDrawPrim( );
		//TestBoolean0( );
		//TestBoolean1( );
		//TestBoolean2( );
		//TestRay0( );
		TestRay1( );

		// push a draw call to tinygl
		// all members of a tgDrawCall *must* be initialized
		if ( verts.size( ) )
		{
			tgDrawCall call;
			call.r = &r;
			call.texture_count = 0;
			call.verts = &verts[ 0 ];
			call.vert_count = verts.size( );
			tgPushDrawCall( ctx, call );
		}

		// flush all draw calls to screen
		// optionally the fb can be NULL or 0 to signify no post-processing fx
		tgFlush( ctx, SwapBuffers, use_post_fx ? &fb : 0 );
		TG_PRINT_GL_ERRORS( );
		verts.clear( );
	}

	tgFreeCtx( ctx );
	glfwDestroyWindow( window );
	glfwTerminate( );

	return 0;
}
