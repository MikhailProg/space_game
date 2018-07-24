#include <SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <err.h>

#define ARRSZ(a)	(sizeof((a))/sizeof((a)[0]))
#define W		640
#define H		480
#define D		(W / 2)
#define NEAR		20
#define FAR		2000
#define RANGE(a, b)	((a) + rand() % (int)(b))
#define VELOCITY	4
#define MIN(a, b)	((a) < (b) ? (a) : (b))
#define MAX(a, b)	((a) > (b) ? (a) : (b))

typedef struct Point Point;
typedef struct Point Vec;
typedef struct Ship Ship;
typedef struct Edge Edge;
typedef struct Expl Expl;

struct Point {
	float	x, y, z;
};

struct Edge {
	int	v1, v2;
};

struct Ship {
	int	alive;
	float 	x,   y,  z;
	float	vx, vy, vz;
};

struct Expl {
	int	alive;
	int	span;
	Point	p1[8];
	Point	p2[8];
	Vec	vel[8];
};

enum {
	TRIGGER_READY,
	TRIGGER_FIRE,
	TRIGGER_COOL
};

static Point		stars[1024];
static Ship		ships[10];
static Expl		expls[10];
static SDL_Surface	*image;
static int		velocity = VELOCITY;
static int		trigger;
static int		trigger_state = TRIGGER_READY;
static int		trigger_count;
static int		target_x = W / 2, target_y = H / 2;
static int		cross_move_x, cross_move_y;
static int		hits;

static Point		verts[] = {
	{ -40,  40, 0 }, { -40,   0, 0 },
	{ -40, -40, 0 }, { -10,   0, 0 },
	{   0,  20, 0 }, {  10,   0, 0 },
	{   0, -20, 0 }, {  40,  40, 0 },
	{  40,   0, 0 }, {  40, -40, 0 }
};

static Edge		edges[] = {
	{ 0, 2 }, { 1, 3 },
	{ 3, 4 }, { 4, 5 },
	{ 5, 6 }, { 6, 3 },
	{ 5, 8 }, { 7, 9 }
};


static void line_draw(int x1, int y1, int x2, int y2,
		      unsigned char r, unsigned char g, unsigned char b)
{
	int dx = abs(x2 - x1), sx = x1 < x2 ? 1 : -1;
	int dy = abs(y2 - y1), sy = y1 < y2 ? 1 : -1; 
	int err = (dx > dy ? dx : -dy) / 2, e2;
	unsigned char *pix;
	int stride = image->pitch;

	for (;;) {
		if (!(x1 < 0 || x1 >= W || y1 < 0 || y1 >= H)) {
			pix = image->pixels;
			pix += y1 * stride + 3 * x1;
			pix[2] = r;
			pix[1] = g;
			pix[0] = b;
		}

		if (x1 == x2 && y1 == y2)
			break;
		e2 = err;

		if (e2 >-dx) {
			err -= dy;
			x1 += sx;
		}
		if (e2 < dy) {
			err += dx;
			y1 += sy;
		}
	}
}

static void expl_init(const Ship *s)
{
	Expl *e;
	int i;

	for (i = 0; i < (int)ARRSZ(expls); i++)
		if (!expls[i].alive)
			break;

	if (i == (int)ARRSZ(expls))
		return;

	e = &expls[i];
	e->alive = 1;
	e->span  = 0;

	for (i = 0; i < (int)ARRSZ(edges); i++) {
		e->p1[i].x = verts[edges[i].v1].x + s->x;
		e->p1[i].y = verts[edges[i].v1].y + s->y;
		e->p1[i].z = verts[edges[i].v1].z + s->z;

		e->p2[i].x = verts[edges[i].v2].x + s->x;
		e->p2[i].y = verts[edges[i].v2].y + s->y;
		e->p2[i].z = verts[edges[i].v2].z + s->z;

		e->vel[i].x = s->vx - RANGE(-8, 16);
		e->vel[i].y = s->vy - RANGE(-8, 16);
		e->vel[i].z = RANGE(-3, 4);
	}
}

static void expl_update(Expl *e)
{
	int i;
		
	if (!e->alive)
		return;

	for (i = 0; i < (int)ARRSZ(edges); i++) {
		e->p1[i].x += e->vel[i].x;
		e->p1[i].y += e->vel[i].y;
		e->p1[i].z += e->vel[i].z;
		e->p2[i].x += e->vel[i].x;
		e->p2[i].y += e->vel[i].y;
		e->p2[i].z += e->vel[i].z;
	}

	if (++e->span > 100)
		e->span = e->alive = 0;
}

static void expl_draw(Expl *e)
{
	float p1x, p1y, p2x, p2y, sc;
	int i, x1, y1, x2, y2;

	if (!e->alive)
		return;
	
	for (i = 0; i < (int)ARRSZ(edges); i++) {
		if (e->p1[i].z < NEAR && e->p2[i].z < NEAR)	
			continue;
		/* Perspective. */
		p1x = D * e->p1[i].x / e->p1[i].z;
		p1y = D * e->p1[i].y / e->p1[i].z;
		p2x = D * e->p2[i].x / e->p2[i].z;
		p2y = D * e->p2[i].y / e->p2[i].z;
		/* Scale factor for the color. */
		sc  = 1. - e->p2[i].z / (4 * FAR);
		sc *= 1. - (float)e->span / 100;
		/* Map to the screen. */
		x1 = W / 2 + p1x; 
		y1 = H / 2 - p1y; 
		x2 = W / 2 + p2x; 
		y2 = H / 2 - p2y; 
		line_draw(x1, y1, x2, y2, 255 * sc, 255 * sc, 0);
	}
}

static void expls_foreach(void (*fn)(Expl *))
{
	Expl *p, *e;

	p = expls;
	e = p + ARRSZ(expls);

	while (p < e)
		fn(p++);
}

static void expls_update(void)
{
	expls_foreach(expl_update);	
}

static void expls_draw(void)
{
	expls_foreach(expl_draw);
}

static void ship_init(Ship *s)
{
	s->x  =  RANGE(-W, 2 * W);
	s->y  =  RANGE(-H, 2 * H);
	s->z  =  4 * FAR;
	s->vx =  RANGE(-4, 8);
	s->vy =  RANGE(-4, 8);
	s->vz = -RANGE( 4, 64);
	
	s->alive = 1;
}

static void ship_update(Ship *s)
{
	s->x += s->vx;
	s->y += s->vy;
	s->z += (s->vz - velocity);

	if (s->z < NEAR)
		ship_init(s);
}

static void ship_draw(Ship *s)
{
	Point v1, v2;
	int i, x1, y1, x2, y2, min_x, min_y, max_x, max_y;
	float sc;

	if (!s->alive)
		return;

	min_x = min_y = +10000;
	max_x = max_y = -10000;

	for (i = 0; i < (int)ARRSZ(edges); i++) {
		v1 = verts[edges[i].v1];
		v2 = verts[edges[i].v2];
		/* Translate. */
		v1.x += s->x;
		v1.y += s->y;
		v1.z += s->z;
		v2.x += s->x;
		v2.y += s->y;
		v2.z += s->z;
		sc = 1. - v2.z / (4 * FAR);
		/* Perspective. */
		v1.x = D * v1.x / v1.z;
		v1.y = D * v1.y / v1.z;
		v2.x = D * v2.x / v2.z;
		v2.y = D * v2.y / v2.z;
		/* Map to the screen. */
		x1 = W / 2 + v1.x; 
		y1 = H / 2 - v1.y; 
		x2 = W / 2 + v2.x; 
		y2 = H / 2 - v2.y; 

		/* Calculate bounding box. */
		min_x = MIN(x2, MIN(x1, min_x));
		min_y = MIN(y2, MIN(y1, min_y));
		max_x = MAX(x2, MAX(x1, max_x));
		max_y = MAX(y2, MAX(y1, max_y));

		if (trigger_state == TRIGGER_FIRE &&
		    target_x > min_x && target_x < max_x &&
		    target_y > min_y && target_y < max_y) {
			s->alive = 0;
			expl_init(s);
			++hits;
			break;
		}

		line_draw(x1, y1, x2, y2, 0, 255 * sc, 0);
	}
}

static void ship_foreach(void (*fn)(Ship *))
{
	Ship *p, *e;

	p = ships;
	e = p + ARRSZ(ships);
	
	while (p < e)
		fn(p++);
}

static void ships_init(void)
{
	ship_foreach(ship_init);
}

static void ships_update(void)
{
	ship_foreach(ship_update);
}

static void ships_draw(void)
{
	ship_foreach(ship_draw);
}

static void stars_init(void)
{
	Point *p, *e;
 
	p = stars;
	e = p + ARRSZ(stars);

	while (p < e) {
		p->x = RANGE(-FAR / 2, FAR);
		p->y = RANGE(-FAR / 2 / 1.3, FAR / 1.3);
		p->z = RANGE(NEAR, FAR - NEAR);
		++p;
	}
}

static void stars_update(void)
{
	Point *p, *e;
 
	p = stars;
	e = p + ARRSZ(stars);

	while (p < e) {
		p->z -= velocity;
		if (p->z < NEAR) {
			p->x = RANGE(-FAR / 2, FAR);
			p->y = RANGE(-FAR / 2 / 1.3, FAR / 1.3);
			p->z = FAR;
		}
		++p;
	}
}

static void stars_draw(void)
{
	Point *p, *e;
	unsigned char *pix;
	int sx, sy, stride;
	float x, y, z, s;
 
	p = stars;
	e = p + ARRSZ(stars);
	stride = image->pitch;

	while (p < e) {
		x = D * p->x / p->z;
		y = D * p->y / p->z;
		z = p->z; 
		++p;

		sx = roundf(W / 2 + x);
		sy = roundf(H / 2 - y);
		if (sx < 0 || sx >= W || sy < 0 || sy >= H || z > FAR)
			continue;

		pix = image->pixels;
		pix += sy * stride + 3 * sx;
		s = (FAR - z) / (FAR - NEAR);
		pix[0] = pix[1] = pix[2] = 255 * s;
	}
}

static void trigger_update(void)
{
	switch (trigger_state) {
	case TRIGGER_READY:
		if (trigger) {
			trigger_count = 0;
			trigger_state = TRIGGER_FIRE;
		}
		break;
	case TRIGGER_FIRE:
		++trigger_count;
		if (trigger_count > 15)
			trigger_state = TRIGGER_COOL;
		break;
	case TRIGGER_COOL:
		++trigger_count;
		if (trigger_count > 20)
			trigger_state = TRIGGER_READY;
		break;
	}
}

static void trigger_draw(void)
{
	int x, y;

	if (trigger_state != TRIGGER_FIRE)
		return;

	x = rand() % 2 == 1 ? 0 : W - 1;
	y = H - 1;

	line_draw(x, y, -4 + rand() % 8 + target_x,
			-4 + rand() % 8 + target_y,
			0, 0, rand() % 256);
}

static void cross_update(void)
{
	const int v = 10;

	if (cross_move_x)
		target_x += v * cross_move_x;

	if (target_x < 0)
		target_x = 0;
	else if (target_x >= W)
		target_x = W - 1;

	if (cross_move_y)
		target_y += v * cross_move_y;

	if (target_y < 0)
		target_y = 0;
	else if (target_y >= H)
		target_y = H - 1;
}

static void cross_draw(void)
{
	line_draw(target_x - 4, target_y, target_x + 4, target_y,
			255, 0, 0);
	line_draw(target_x, target_y - 4, target_x, target_y + 4,
			255, 0, 0);
}

static int event_dispatch(void)
{
	SDL_Event event;
	int q = 0;

	while (SDL_PollEvent(&event)) {
		switch (event.type) {
		case SDL_KEYDOWN:
			switch (event.key.keysym.sym) {
			case SDLK_ESCAPE:
				q = 1;
				break;
			case SDLK_w:
				velocity += 4;
				if (velocity > 24)
					velocity = 24;
				break;
			case SDLK_s:
				velocity -= 4;
				if (velocity < 0)
					velocity = 0;
				break;
			case SDLK_SPACE:
				trigger = 1;
				break;
			case SDLK_LEFT:
				cross_move_x = -1;
				break;
			case SDLK_RIGHT:
				cross_move_x = 1;
				break;
			case SDLK_UP:
				cross_move_y = -1;
				break;
			case SDLK_DOWN:
				cross_move_y = 1;
				break;
			default:
				break;
			}
			break;
		case SDL_KEYUP:
			switch (event.key.keysym.sym) {
			case SDLK_SPACE:
				trigger = 0;
				break;
			case SDLK_LEFT:
			case SDLK_RIGHT:
				cross_move_x = 0;
				break;
			case SDLK_UP:
			case SDLK_DOWN:
				cross_move_y = 0;
				break;
			default:
				break;
			}
			break;
		default:
			break;
		}
	}

	return q;
}

int main(void)
{
	SDL_Surface *screen;
	int flags, q = 0;

	if (SDL_Init(SDL_INIT_VIDEO) < 0)
		errx(1, "SDL_Init: %s", SDL_GetError());

	atexit(SDL_Quit);

	flags = SDL_HWSURFACE | SDL_DOUBLEBUF;
	screen = SDL_SetVideoMode(640, 480, 24, flags);
	if (!screen)
		errx(1, "SDL_SetVideoMode: %s", SDL_GetError());

	image = SDL_CreateRGBSurface(SDL_HWSURFACE, screen->w, screen->h, 24,
				   screen->format->Rmask, screen->format->Gmask,
				   screen->format->Bmask, screen->format->Amask);
	if (!image)
		errx(1, "SDL_CreateRGBSurface: %s", SDL_GetError());

	srand(time(NULL));
	stars_init();
	ships_init();

	while (!q) {
		q = event_dispatch();
		SDL_FillRect(image, NULL, 0);

		stars_update();
		ships_update();
		expls_update();
		trigger_update();
		cross_update();

		stars_draw();
		ships_draw();
		expls_draw();
		trigger_draw();
		cross_draw();

		if (SDL_BlitSurface(image, NULL, screen, NULL) < 0)
			errx(1, "SDL_BlitSurface: %s", SDL_GetError());

		if (SDL_Flip(screen) < 0)
			errx(1, "SDL_Flip: %s", SDL_GetError());

		SDL_Delay(33);
	}

	SDL_FreeSurface(image);
	warnx("HITS: %d", hits);

	return 0;
}

