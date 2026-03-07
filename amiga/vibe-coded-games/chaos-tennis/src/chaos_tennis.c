#include <exec/types.h>
#include <exec/memory.h>
#include <exec/libraries.h>
#include <graphics/gfx.h>
#include <graphics/gfxbase.h>
#include <graphics/rastport.h>
#include <intuition/intuition.h>
#include <dos/dos.h>

#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>

/* Hardware registers for joystick reading */
#define JOY0DAT   (*(volatile UWORD *)0xDFF00A)
#define JOY1DAT   (*(volatile UWORD *)0xDFF00C)
#define CIAAPRA   (*(volatile UBYTE *)0xBFE001)

#define SCREEN_WIDTH   320
#define SCREEN_HEIGHT  256
#define SCREEN_DEPTH   4      /* 16 colours */

#define PAD_WIDTH      6
#define PAD_HEIGHT     40
#define PAD_MARGIN     8
#define PAD_SPEED      3

#define BALL_SIZE      4

#define MAX_OBJECTS    6

#define WIN_SCORE      5

enum ChaosType {
    CHAOS_SPEED_UP = 0,
    CHAOS_SLOW_DOWN,
    CHAOS_ANGLE,
    CHAOS_TELEPORT,
    CHAOS_TYPE_COUNT
};

struct ChaosObject {
    WORD x, y;
    WORD dx, dy;
    enum ChaosType type;
};

struct JoyState {
    UBYTE up;
    UBYTE down;
    UBYTE left;
    UBYTE right;
    UBYTE fire;
};

struct GfxBase *GfxBase = NULL;
struct IntuitionBase *IntuitionBase = NULL;

static struct Screen *gameScreen = NULL;

static ULONG rng_state = 0x12345678;

static ULONG
rnd(void)
{
    /* Simple 32-bit LCG */
    rng_state = rng_state * 1664525UL + 1013904223UL;
    return rng_state;
}

static WORD
rand_range(WORD min, WORD max)
{
    if (max <= min) return min;
    return (WORD)(min + (rnd() % (ULONG)(max - min + 1)));
}

static void
read_joystick(WORD port, struct JoyState *st)
{
    UWORD joydat;
    UBYTE pra = CIAAPRA;
    UBYTE fire;
    UBYTE right, left, back, forward;

    if (port == 0)
        joydat = JOY0DAT;
    else
        joydat = JOY1DAT;

    /* Fire buttons on CIAAPRA bits 6 (port 0) and 7 (port 1), 0 = pressed */
    if (port == 0)
        fire = (pra & (1 << 6)) ? 0 : 1;
    else
        fire = (pra & (1 << 7)) ? 0 : 1;

    /* Decode digital joystick directions from JOYxDAT
       See Amiga HW manual: Table 8-3 */
    right   = (joydat >> 1) & 1;                            /* bit 1 */
    left    = (joydat >> 9) & 1;                            /* bit 9 */
    back    = (UBYTE)((((joydat >> 1) ^ joydat) & 1));      /* bit1 XOR bit0 */
    forward = (UBYTE)((((joydat >> 9) ^ (joydat >> 8)) & 1)); /* bit9 XOR bit8 */

    st->up    = forward;
    st->down  = back;
    st->left  = left;
    st->right = right;
    st->fire  = fire;
}

static void
clear_screen(struct RastPort *rp)
{
    SetAPen(rp, 0);
    RectFill(rp, 0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1);
}

static void
draw_paddle(struct RastPort *rp, WORD x, WORD y, UBYTE pen)
{
    SetAPen(rp, pen);
    RectFill(rp,
             x,
             y,
             x + PAD_WIDTH - 1,
             y + PAD_HEIGHT - 1);
}

static void
draw_ball(struct RastPort *rp, WORD x, WORD y, UBYTE pen)
{
    SetAPen(rp, pen);
    RectFill(rp,
             x,
             y,
             x + BALL_SIZE - 1,
             y + BALL_SIZE - 1);
}

static void
draw_object(struct RastPort *rp, const struct ChaosObject *obj)
{
    UBYTE pen;

    switch (obj->type) {
    case CHAOS_SPEED_UP:   pen = 2;  break;
    case CHAOS_SLOW_DOWN:  pen = 3;  break;
    case CHAOS_ANGLE:      pen = 4;  break;
    case CHAOS_TELEPORT:   pen = 5;  break;
    default:               pen = 1;  break;
    }

    RectFill(rp,
             obj->x - 3,
             obj->y - 3,
             obj->x + 3,
             obj->y + 3);
}

static void
init_palette(void)
{
    /* Simple colourful palette using RGB4 values */
    UWORD colors[16 * 3];
    WORD i;

    for (i = 0; i < 16 * 3; ++i)
        colors[i] = 0;

    /* Background: dark blue */
    colors[0 * 3 + 0] = 0x0;
    colors[0 * 3 + 1] = 0x0;
    colors[0 * 3 + 2] = 0x3;

    /* Paddles / ball */
    colors[1 * 3 + 0] = 0xF;
    colors[1 * 3 + 1] = 0xF;
    colors[1 * 3 + 2] = 0xF;

    /* Chaos types */
    colors[2 * 3 + 0] = 0xF; colors[2 * 3 + 1] = 0x0; colors[2 * 3 + 2] = 0x0; /* red */
    colors[3 * 3 + 0] = 0x0; colors[3 * 3 + 1] = 0xF; colors[3 * 3 + 2] = 0x0; /* green */
    colors[4 * 3 + 0] = 0x0; colors[4 * 3 + 1] = 0x0; colors[4 * 3 + 2] = 0xF; /* blue */
    colors[5 * 3 + 0] = 0xF; colors[5 * 3 + 1] = 0xF; colors[5 * 3 + 2] = 0x0; /* yellow */

    /* Score / text */
    colors[6 * 3 + 0] = 0xF; colors[6 * 3 + 1] = 0x8; colors[6 * 3 + 2] = 0x0; /* orange */
    colors[7 * 3 + 0] = 0x8; colors[7 * 3 + 1] = 0x0; colors[7 * 3 + 2] = 0xF; /* violet */

    LoadRGB4(&gameScreen->ViewPort, colors, 16);
}

static void
draw_center_line(struct RastPort *rp)
{
    WORD y;
    SetAPen(rp, 1);
    for (y = 0; y < SCREEN_HEIGHT; y += 8) {
        Move(rp, SCREEN_WIDTH / 2, y);
        Draw(rp, SCREEN_WIDTH / 2, y + 4);
    }
}

static void
draw_scores(struct RastPort *rp, WORD scoreLeft, WORD scoreRight)
{
    char buf[8];

    SetAPen(rp, 6);

    /* Left score */
    buf[0] = '0' + (char)scoreLeft;
    buf[1] = '\0';
    Move(rp, SCREEN_WIDTH / 2 - 40, 12);
    Text(rp, buf, 1);

    /* Right score */
    buf[0] = '0' + (char)scoreRight;
    Move(rp, SCREEN_WIDTH / 2 + 32, 12);
    Text(rp, buf, 1);
}

static void
reset_objects(struct ChaosObject *objs)
{
    WORD i;
    for (i = 0; i < MAX_OBJECTS; ++i) {
        objs[i].type = (enum ChaosType)(rnd() % CHAOS_TYPE_COUNT);
        objs[i].x = rand_range(SCREEN_WIDTH / 2 - 40, SCREEN_WIDTH / 2 + 40);
        objs[i].y = rand_range(32, SCREEN_HEIGHT - 32);
        objs[i].dx = (rnd() & 1) ? 1 : -1;
        objs[i].dy = (rnd() & 1) ? 1 : -1;
    }
}

static void
update_objects(struct ChaosObject *objs)
{
    WORD i;
    for (i = 0; i < MAX_OBJECTS; ++i) {
        struct ChaosObject *o = &objs[i];

        o->x += o->dx;
        o->y += o->dy;

        if (o->x < SCREEN_WIDTH / 2 - 60 || o->x > SCREEN_WIDTH / 2 + 60)
            o->dx = -o->dx;
        if (o->y < 32 || o->y > SCREEN_HEIGHT - 32)
            o->dy = -o->dy;
    }
}

static WORD
point_in_rect(WORD px, WORD py,
              WORD rx, WORD ry, WORD rw, WORD rh)
{
    return (px >= rx && px < rx + rw &&
            py >= ry && py < ry + rh);
}

static void
apply_object_effect(const struct ChaosObject *obj,
                    float *vx, float *vy, WORD *bx, WORD *by)
{
    switch (obj->type) {
    case CHAOS_SPEED_UP:
        *vx *= 1.25f;
        *vy *= 1.25f;
        break;
    case CHAOS_SLOW_DOWN:
        *vx *= 0.8f;
        *vy *= 0.8f;
        if (*vx > -1.0f && *vx < 1.0f)
            *vx = (*vx >= 0.f) ? 1.0f : -1.0f;
        break;
    case CHAOS_ANGLE:
        *vy = -*vy + ((float)rand_range(-2, 2) * 0.3f);
        break;
    case CHAOS_TELEPORT:
        *bx = rand_range(40, SCREEN_WIDTH - 40);
        *by = rand_range(40, SCREEN_HEIGHT - 40);
        break;
    default:
        break;
    }
}

static void
draw_intro(struct RastPort *rp, WORD frame)
{
    const char *title = "CHAOS TENNIS";
    WORD len = 12;
    WORD x = (SCREEN_WIDTH - len * 16) / 2;
    WORD y = SCREEN_HEIGHT / 3;
    WORD i;

    clear_screen(rp);

    /* Rainbow title letters */
    for (i = 0; i < len; ++i) {
        UBYTE pen = (UBYTE)(2 + ((i + (frame / 4)) % 5));
        SetAPen(rp, pen);
        Move(rp, x + i * 16, y);
        Text(rp, &title[i], 1);
    }

    SetAPen(rp, 6);
    Move(rp, SCREEN_WIDTH / 2 - 80, y + 40);
    Text(rp, "Press fire to start", 18);
}

static void
show_winner(struct RastPort *rp, WORD winner)
{
    char buf[32];
    WORD len;

    clear_screen(rp);

    SetAPen(rp, 6);
    Move(rp, SCREEN_WIDTH / 2 - 70, SCREEN_HEIGHT / 3);

    if (winner == 1) {
        const char *msg = "PLAYER 1 WINS!";
        Text(rp, msg, 14);
    } else if (winner == 2) {
        const char *msg = "PLAYER 2 WINS!";
        Text(rp, msg, 14);
    } else {
        const char *msg = "GAME OVER";
        Text(rp, msg, 9);
    }

    Move(rp, SCREEN_WIDTH / 2 - 90, SCREEN_HEIGHT / 3 + 40);
    len = 25;
    buf[0] = '\0'; /* silence compiler about buf; we use constant string below */
    Text(rp, "Press fire to return", len);
}

static WORD
wait_for_fire_or_quit(void)
{
    struct RastPort *rp = &gameScreen->RastPort;
    struct JoyState j0, j1;

    for (;;) {
        read_joystick(0, &j0);
        read_joystick(1, &j1);

        if (j0.fire || j1.fire)
            return 1;

        /* Allow Ctrl-C abort if run from CLI */
        if (SetSignal(0L, 0L) & SIGBREAKF_CTRL_C)
            return 0;

        WaitTOF();
        /* Keep copperlist updated */
        MakeScreen(gameScreen);
        RethinkDisplay();

        (void)rp; /* rp unused but kept for symmetry */
    }
}

static WORD
intro_screen(void)
{
    struct RastPort *rp = &gameScreen->RastPort;
    struct JoyState j0, j1;
    WORD frame = 0;

    for (;;) {
        read_joystick(0, &j0);
        read_joystick(1, &j1);

        if (j0.fire || j1.fire)
            return 1;

        if (SetSignal(0L, 0L) & SIGBREAKF_CTRL_C)
            return 0;

        draw_intro(rp, frame++);

        WaitTOF();
        MakeScreen(gameScreen);
        RethinkDisplay();
    }
}

static WORD
play_match(void)
{
    struct RastPort *rp = &gameScreen->RastPort;
    struct JoyState j0, j1;
    struct ChaosObject objs[MAX_OBJECTS];

    WORD padLeftY  = (SCREEN_HEIGHT - PAD_HEIGHT) / 2;
    WORD padRightY = (SCREEN_HEIGHT - PAD_HEIGHT) / 2;

    WORD ballX = SCREEN_WIDTH / 2;
    WORD ballY = SCREEN_HEIGHT / 2;
    float vx = 2.0f;
    float vy = 1.0f;

    WORD scoreLeft = 0;
    WORD scoreRight = 0;

    reset_objects(objs);

    for (;;) {
        read_joystick(0, &j0);
        read_joystick(1, &j1);

        if (SetSignal(0L, 0L) & SIGBREAKF_CTRL_C)
            return 0;

        /* Paddles: up/down only */
        if (j0.up && padLeftY > PAD_MARGIN)
            padLeftY -= PAD_SPEED;
        if (j0.down && padLeftY < SCREEN_HEIGHT - PAD_MARGIN - PAD_HEIGHT)
            padLeftY += PAD_SPEED;

        if (j1.up && padRightY > PAD_MARGIN)
            padRightY -= PAD_SPEED;
        if (j1.down && padRightY < SCREEN_HEIGHT - PAD_MARGIN - PAD_HEIGHT)
            padRightY += PAD_SPEED;

        /* Update ball */
        ballX += (WORD)vx;
        ballY += (WORD)vy;

        /* Bounce off top/bottom */
        if (ballY <= 8) {
            ballY = 8;
            vy = -vy;
        } else if (ballY >= SCREEN_HEIGHT - 8) {
            ballY = SCREEN_HEIGHT - 8;
            vy = -vy;
        }

        /* Paddle collisions */
        if (ballX <= PAD_MARGIN + PAD_WIDTH &&
            point_in_rect(ballX, ballY,
                          PAD_MARGIN, padLeftY,
                          PAD_WIDTH, PAD_HEIGHT)) {
            ballX = PAD_MARGIN + PAD_WIDTH + 1;
            vx = (vx < 0) ? -vx : vx;
            vy += ((float)(ballY - (padLeftY + PAD_HEIGHT / 2))) * 0.05f;
        }

        if (ballX >= SCREEN_WIDTH - PAD_MARGIN - PAD_WIDTH - BALL_SIZE &&
            point_in_rect(ballX, ballY,
                          SCREEN_WIDTH - PAD_MARGIN - PAD_WIDTH,
                          padRightY,
                          PAD_WIDTH, PAD_HEIGHT)) {
            ballX = SCREEN_WIDTH - PAD_MARGIN - PAD_WIDTH - BALL_SIZE - 1;
            vx = (vx > 0) ? -vx : vx;
            vy += ((float)(ballY - (padRightY + PAD_HEIGHT / 2))) * 0.05f;
        }

        /* Check goal */
        if (ballX < 0) {
            scoreRight++;
            ballX = SCREEN_WIDTH / 2;
            ballY = SCREEN_HEIGHT / 2;
            vx = 2.0f;
            vy = (rnd() & 1) ? 1.0f : -1.0f;
            reset_objects(objs);
        } else if (ballX > SCREEN_WIDTH - BALL_SIZE) {
            scoreLeft++;
            ballX = SCREEN_WIDTH / 2;
            ballY = SCREEN_HEIGHT / 2;
            vx = -2.0f;
            vy = (rnd() & 1) ? 1.0f : -1.0f;
            reset_objects(objs);
        }

        if (scoreLeft >= WIN_SCORE)
            return 1;
        if (scoreRight >= WIN_SCORE)
            return 2;

        /* Chaos objects */
        update_objects(objs);

        {
            WORD i;
            for (i = 0; i < MAX_OBJECTS; ++i) {
                if (point_in_rect(ballX, ballY,
                                  objs[i].x - 4, objs[i].y - 4,
                                  8, 8)) {
                    apply_object_effect(&objs[i], &vx, &vy, &ballX, &ballY);
                }
            }
        }

        /* Drawing */
        clear_screen(rp);
        draw_center_line(rp);
        draw_scores(rp, scoreLeft, scoreRight);
        draw_paddle(rp, PAD_MARGIN, padLeftY, 1);
        draw_paddle(rp, SCREEN_WIDTH - PAD_MARGIN - PAD_WIDTH, padRightY, 1);
        draw_ball(rp, ballX, ballY, 1);

        {
            WORD i;
            for (i = 0; i < MAX_OBJECTS; ++i)
                draw_object(rp, &objs[i]);
        }

        WaitTOF();
        MakeScreen(gameScreen);
        RethinkDisplay();
    }
}

int
main(void)
{
    struct NewScreen ns;
    WORD running = 1;

    GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 0);
    if (!GfxBase)
        return RETURN_FAIL;

    IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 0);
    if (!IntuitionBase) {
        CloseLibrary((struct Library *)GfxBase);
        return RETURN_FAIL;
    }

    /* Set up a simple low-res 16 colour screen */
    ns.LeftEdge   = 0;
    ns.TopEdge    = 0;
    ns.Width      = SCREEN_WIDTH;
    ns.Height     = SCREEN_HEIGHT;
    ns.Depth      = SCREEN_DEPTH;
    ns.DetailPen  = 0;
    ns.BlockPen   = 1;
    ns.ViewModes  = 0;
    ns.Type       = CUSTOMSCREEN;
    ns.Font       = NULL;
    ns.DefaultTitle = (UBYTE *)"CHAOS TENNIS";
    ns.Gadgets    = NULL;
    ns.CustomBitMap = NULL;

    gameScreen = OpenScreen(&ns);
    if (!gameScreen) {
        CloseLibrary((struct Library *)IntuitionBase);
        CloseLibrary((struct Library *)GfxBase);
        return RETURN_FAIL;
    }

    init_palette();

    while (running) {
        WORD ok;
        WORD winner;

        ok = intro_screen();
        if (!ok)
            break;

        winner = play_match();
        if (!winner) {
            /* User aborted with Ctrl-C */
            break;
        }

        show_winner(&gameScreen->RastPort, winner);
        if (!wait_for_fire_or_quit())
            break;
    }

    CloseScreen(gameScreen);
    CloseLibrary((struct Library *)IntuitionBase);
    CloseLibrary((struct Library *)GfxBase);

    return RETURN_OK;
}

