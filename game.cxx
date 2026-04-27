#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <iostream>
#include <cmath>
#include <vector>
#include <ctime>
#include <string>

int SW = 1280, SH = 720;

// ------------------------- PROJECTILE (POMP) -------------------------
class Projectile
{
  public:
    float x, y, vx, vy, startX, startY, targetRange;
    bool expired = false;
    SDL_Texture *tex;

    Projectile(float _x, float _y, float angle, float range, SDL_Texture *_tex)
    {
        x = _x;
        y = _y;
        startX = _x;
        startY = _y;
        targetRange = range;
        tex = _tex;
        float speed = 45.0f; // سرعة انطلاق الصاروخ
        vx = cos((angle - 90) * M_PI / 180.0f) * speed;
        vy = sin((angle - 90) * M_PI / 180.0f) * speed;
    }

    void update()
    {
        x += vx;
        y += vy;
        float distTraveled = sqrt(pow(x - startX, 2) + pow(y - startY, 2));
        if (distTraveled >= targetRange)
            expired = true;
    }

    void draw(SDL_Renderer *renderer, float camX, float camY, float aimAngle)
    {
        float rad = (aimAngle - 145) * M_PI / 180.0f;
        SDL_FRect rect = {x - camX - 20, y - camY - 20, 40, 40};
            SDL_RenderCopyExF(renderer, tex, NULL, &rect,aimAngle, NULL, SDL_FLIP_NONE);
        SDL_RenderCopyF(renderer, tex, NULL, &rect);
    }
};

// ------------------------- PARTICLES (SMOKE) -------------------------
class Particle
{
  public:
    float x, y, vx, vy, size;
    int alpha;
    bool expired = false;

    Particle(float _x, float _y)
    {
        x = _x;
        y = _y;
        vx = (rand() % 20 - 10) / 10.0f;
        vy = (rand() % 20 - 10) / 10.0f;
        size = rand() % 15 + 10;
        alpha = 150;
    }

    void update()
    {
        x += vx;
        y += vy;
        alpha -= 5;
        size += 0.2f;
        if (alpha <= 0)
            expired = true;
    }

    void draw(SDL_Renderer *renderer, float camX, float camY)
    {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 150, 150, 150, alpha);
        SDL_FRect rect = {x - camX, y - camY, size, size};
        SDL_RenderFillRectF(renderer, &rect);
    }
};

// ------------------------- BLOOD -------------------------
class BloodStain
{
  public:
    float x, y, w, h, angle;
    Uint32 spawnTime;
    int alpha = 255;
    bool expired = false;

    BloodStain(float _x, float _y, float _w, float _h)
    {
        x = _x;
        y = _y;
        w = _w;
        h = _h;
        angle = rand() % 360;
        spawnTime = SDL_GetTicks();
    }

    void update()
    {
        Uint32 elapsed = SDL_GetTicks() - spawnTime;
        if (elapsed > 4000)
            expired = true;
        alpha = 255 - (int)((elapsed / 4000.0f) * 255);
    }

    void draw(SDL_Renderer *renderer, SDL_Texture *tex, float camX, float camY)
    {
        SDL_SetTextureAlphaMod(tex, alpha);
        SDL_FRect rect = {x - camX, y - camY, w, h};
        SDL_RenderCopyExF(renderer, tex, NULL, &rect, angle, NULL, SDL_FLIP_NONE);
        SDL_SetTextureAlphaMod(tex, 255);
    }
};

// ------------------------- JOYSTICK -------------------------
class Joystick
{
  public:
    float baseX, baseY, baseRadius, knobX, knobY, knobRadius;
    bool isTouching = false;
    int fingerId = -1;
    float angle = 0, powerRatio = 0; // powerRatio لمعرفة مدى سحب العصا (0 إلى 1)

    Joystick(float x, float y, float r)
    {
        baseX = x;
        baseY = y;
        baseRadius = r;
        knobX = x;
        knobY = y;
        knobRadius = r / 2.2f;
    }

    void handleEvent(SDL_Event &e)
    {
        if (e.type == SDL_FINGERDOWN || e.type == SDL_FINGERMOTION)
        {
            float tx = e.tfinger.x * SW, ty = e.tfinger.y * SH;
            float dx = tx - baseX, dy = ty - baseY;
            float dist = sqrt(dx * dx + dy * dy);

            if (e.type == SDL_FINGERDOWN && dist < baseRadius * 1.5f && fingerId == -1)
            {
                isTouching = true;
                fingerId = e.tfinger.fingerId;
            }
            if (isTouching && e.tfinger.fingerId == fingerId)
            {
                float clampedDist = std::min(dist, baseRadius);
                powerRatio = clampedDist / baseRadius; // نسبة السحب
                if (dist > 0)
                {
                    knobX = baseX + (dx / dist) * clampedDist;
                    knobY = baseY + (dy / dist) * clampedDist;
                    angle = (atan2(dy, dx) * 180 / M_PI) + 90;
                }
            }
        }
        else if (e.type == SDL_FINGERUP && e.tfinger.fingerId == fingerId)
        {
            isTouching = false;
            fingerId = -1;
            knobX = baseX;
            knobY = baseY;
        }
    }

    void draw(SDL_Renderer *renderer)
    {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 40);
        SDL_Rect bR = {(int)(baseX - baseRadius), (int)(baseY - baseRadius), (int)(baseRadius * 2), (int)(baseRadius * 2)};
        SDL_RenderFillRect(renderer, &bR);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 120);
        SDL_Rect kR = {(int)(knobX - knobRadius), (int)(knobY - knobRadius), (int)(knobRadius * 2), (int)(knobRadius * 2)};
        SDL_RenderFillRect(renderer, &kR);
    }
};

// ------------------------- CAR -------------------------
class Car
{
  public:
    float x, y, w, h, velX = 0, velY = 0, angle = 0;
    float acceleration = 0.8f, frictionForward = 0.97f, frictionSide = 0.85f;
    SDL_Texture *texture;

    Car(float _x, float _y, SDL_Texture *tex)
    {
        x = _x;
        y = _y;
        w = 150;
        h = 180;
        texture = tex;
    }

    void update(bool isTouching, float jBX, float jBY, float kX, float kY, std::vector<Particle> &particles)
    {
        float oldAngle = angle;
        if (isTouching)
        {
            float dx = kX - jBX, dy = kY - jBY;
            float dist = sqrt(dx * dx + dy * dy);
            if (dist > 5)
            {
                velX += (dx / dist) * acceleration;
                velY += (dy / dist) * acceleration;
                angle = (atan2(dy, dx) * 180 / M_PI) + 90;
            }
        }
        if (std::abs(angle - oldAngle) > 5 && (std::abs(velX) + std::abs(velY)) > 5)
        {
            particles.push_back(Particle(x + w / 2, y + h / 2));
        }
        float rad = (angle - 90) * M_PI / 180.0f;
        float fDirX = cos(rad), fDirY = sin(rad), sDirX = -sin(rad), sDirY = cos(rad);
        float fVel = velX * fDirX + velY * fDirY, sVel = velX * sDirX + velY * sDirY;
        fVel *= frictionForward;
        sVel *= frictionSide;
        velX = fVel * fDirX + sVel * sDirX;
        velY = fVel * fDirY + sVel * sDirY;
        x += velX;
        y += velY;
    }

    void draw(SDL_Renderer *renderer, float camX, float camY, bool isAiming, float aimAngle, float power, SDL_Texture *arrowTex)
    {
        SDL_FRect rect = {x - camX, y - camY, w, h};
        SDL_RenderCopyExF(renderer, texture, NULL, &rect, angle, NULL, SDL_FLIP_NONE);

        if (isAiming)
        {
            float maxRange = 600.0f;
            float currentRange = power * maxRange;
            float rad = (aimAngle - 90) * M_PI / 180.0f;

            // رسم السهم المتمطط
            SDL_FRect arrowRect = {x + w / 2 - camX - 25, y + h / 2 - camY - currentRange, 50, currentRange};
            SDL_FPoint center = {5, currentRange}; // نقطة الارتكاز هي أسفل السهم
            SDL_RenderCopyExF(renderer, arrowTex, NULL, &arrowRect, aimAngle, &center, SDL_FLIP_NONE);

            // رسم دائرة الهدف (Reticle)
            float tx = (x + w / 2) + cos(rad) * currentRange;
            float ty = (y + h / 2) + sin(rad) * currentRange;

            // رسم مربع الهدف الخارجي
            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
            SDL_Rect outerReticle = {(int)(tx - camX - 15), (int)(ty - camY - 15), 30, 30};
            SDL_RenderDrawRect(renderer, &outerReticle);

            // رسم مربع الهدف الداخلي
            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 200);
            SDL_Rect innerReticle = {(int)(tx - camX - 10), (int)(ty - camY - 10), 20, 20};
            SDL_RenderFillRect(renderer, &innerReticle);
        }
    }
};

// ------------------------- ZOMBIE -------------------------
class Zombie
{
  public:
    float x, y, w, h, angle = 0, speed = 1.3f;
    int currentFrame = 0, frameCounter = 0;
    bool active = true;

    Zombie(float _x, float _y)
    {
        x = _x;
        y = _y;
        w = 85;
        h = 85;
        angle = rand() % 360;
    }

    void respawn(float camX, float camY)
    {
        active = true;
        int side = rand() % 4;
        if (side == 0)
        {
            x = camX - 200;
            y = camY + (rand() % SH);
        }
        else if (side == 1)
        {
            x = camX + SW + 200;
            y = camY + (rand() % SH);
        }
        else if (side == 2)
        {
            x = camX + (rand() % SW);
            y = camY - 200;
        }
        else
        {
            x = camX + (rand() % SW);
            y = camY + SH + 200;
        }
        angle = rand() % 360;
    }

    bool checkCollision(float ox, float oy, float ow, float oh)
    {
        if (active && x < ox + ow && x + w > ox && y < oy + oh && y + h > oy)
        {
            active = false;
            return true;
        }
        return false;
    }

    void update()
    {
        if (!active)
            return;
        x += cos(angle * M_PI / 180.0f) * speed;
        y += sin(angle * M_PI / 180.0f) * speed;
        if (rand() % 100 < 2)
            angle += (rand() % 40 - 20);
        frameCounter++;
        if (frameCounter > 8)
        {
            currentFrame = (currentFrame + 1) % 5;
            frameCounter = 0;
        }
    }

    void draw(SDL_Renderer *renderer, SDL_Texture *frames[], float camX, float camY)
    {
        if (!active)
            return;
        SDL_FRect rect = {x - camX, y - camY, w, h};
        SDL_RenderCopyExF(renderer, frames[currentFrame], NULL, &rect, angle, NULL, SDL_FLIP_NONE);
    }
};

// ------------------------- PLAYER -------------------------
class Player
{
  public:
    float x, y, w, h, speed = 4.5f, angle = 0;
    bool alive = true;
    int currentFrame = 0, frameCounter = 0;
    float health = 1.0f;
    Uint32 lastHitTime = 0;
    SDL_Texture *idleTex;
    SDL_Texture *walkFrames[4];

    Player(float _x, float _y, SDL_Texture *idle, SDL_Texture *walk[])
    {
        x = _x;
        y = _y;
        w = 90;
        h = 110;
        idleTex = idle;
        for (int i = 0; i < 4; i++)
            walkFrames[i] = walk[i];
    }

    void update(bool moving, float dx, float dy)
    {
        if (!alive)
            return;
        if (moving)
        {
            x += dx * speed;
            y += dy * speed;
            angle = (atan2(dy, dx) * 180 / M_PI) + 90;
            frameCounter++;
            if (frameCounter > 8)
            {
                currentFrame = (currentFrame + 1) % 4;
                frameCounter = 0;
            }
        }
        else
            currentFrame = 0;
    }

    void takeDamage()
    {
        if (SDL_GetTicks() - lastHitTime > 1000)
        {
            health -= 0.20f;
            lastHitTime = SDL_GetTicks();
        }
        if (health <= 0)
            alive = false;
    }

    void draw(SDL_Renderer *renderer, float camX, float camY, bool moving)
    {
        SDL_FRect rect = {x - camX, y - camY, w, h};
        if (moving)
            SDL_RenderCopyExF(renderer, walkFrames[currentFrame], NULL, &rect, angle, NULL, SDL_FLIP_NONE);
        else
            SDL_RenderCopyExF(renderer, idleTex, NULL, &rect, angle, NULL, SDL_FLIP_NONE);
    }

    void drawHealthBar(SDL_Renderer *renderer)
    {
        SDL_Rect bg = {SW / 2 - 150, 20, 300, 20};
        SDL_SetRenderDrawColor(renderer, 50, 0, 0, 200);
        SDL_RenderFillRect(renderer, &bg);
        SDL_Rect hp = {SW / 2 - 150, 20, (int)(300 * health), 20};
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
        SDL_RenderFillRect(renderer, &hp);
    }
};

// ------------------------- MAIN -------------------------
int main(int argc, char *argv[])
{
    srand(time(0));
    SDL_Init(SDL_INIT_VIDEO);
    IMG_Init(IMG_INIT_PNG);

    SDL_DisplayMode DM;
    SDL_GetCurrentDisplayMode(0, &DM);
    SW = DM.w;
    SH = DM.h;
    SDL_Window *window = SDL_CreateWindow("City Drifter Zombie", 0, 0, SW, SH, SDL_WINDOW_FULLSCREEN);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    SDL_Texture *zombieFrames[5];
    for (int i = 0; i < 5; i++)
        zombieFrames[i] = IMG_LoadTexture(renderer, ("zombie" + std::to_string(i + 1) + ".png").c_str());
    SDL_Texture *carTex = IMG_LoadTexture(renderer, "car.png");
    SDL_Texture *bgTex = IMG_LoadTexture(renderer, "track.png");
    SDL_Texture *bloodTex = IMG_LoadTexture(renderer, "blood.png");
    SDL_Texture *pIdle = IMG_LoadTexture(renderer, "player_idle.png");
    SDL_Texture *pWalk[4];
    for (int i = 0; i < 4; i++)
        pWalk[i] = IMG_LoadTexture(renderer, ("walk" + std::to_string(i + 1) + ".png").c_str());

    // تحميل الصور الجديدة
    SDL_Texture *arrowTex = IMG_LoadTexture(renderer, "arrw.png");
    SDL_Texture *pompTex = IMG_LoadTexture(renderer, "pomp.png");

    Car myCar(0, 0, carTex);
    Joystick moveJoy(200, SH - 200, 110);
    Joystick aimJoy(SW - 200, SH - 200, 110);
    Player player(0, 0, pIdle, pWalk);

    bool inCar = true;
    std::vector<Zombie> zombies;
    std::vector<BloodStain> bloods;
    std::vector<Particle> particles;
    std::vector<Projectile> projectiles;
    for (int i = 0; i < 40; i++)
        zombies.push_back(Zombie(rand() % 4000 - 2000, rand() % 4000 - 2000));

    SDL_Rect btn = {SW / 2 - 80, 50, 160, 50};
    bool running = true;
    SDL_Event e;

    while (running)
    {
        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_QUIT)
                running = false;
            bool wasAiming = aimJoy.isTouching;
            moveJoy.handleEvent(e);
            aimJoy.handleEvent(e);

            if (inCar && wasAiming && !aimJoy.isTouching)
            {
                float range = aimJoy.powerRatio * 800.0f; // المدى يعتمد على سحب العصا
                if (range > 30)
                { // منع الإطلاق إذا كان السحب ضعيفاً جداً
                    projectiles.push_back(Projectile(myCar.x + myCar.w / 2, myCar.y + myCar.h / 2, aimJoy.angle, range, pompTex));
                }
            }

            if (e.type == SDL_FINGERDOWN)
            {
                float tx = e.tfinger.x * SW, ty = e.tfinger.y * SH;
                if (tx > btn.x && tx < btn.x + btn.w && ty > btn.y && ty < btn.y + btn.h)
                {
                    if (inCar)
                    {
                        player.x = myCar.x;
                        player.y = myCar.y;
                        inCar = false;
                    }
                    else if (sqrt(pow(player.x - myCar.x, 2) + pow(player.y - myCar.y, 2)) < 150)
                        inCar = true;
                }
            }
        }

        if (inCar)
            myCar.update(moveJoy.isTouching, moveJoy.baseX, moveJoy.baseY, moveJoy.knobX, moveJoy.knobY, particles);
        else
        {
            float dx = moveJoy.knobX - moveJoy.baseX, dy = moveJoy.knobY - moveJoy.baseY, dist = sqrt(dx * dx + dy * dy);
            if (dist > 10)
                player.update(true, dx / dist, dy / dist);
            else
                player.update(false, 0, 0);
        }

        float camX = (inCar ? myCar.x : player.x) - SW / 2.0f;
        float camY = (inCar ? myCar.y : player.y) - SH / 2.0f;

        for (int i = 0; i < (int)projectiles.size(); i++)
        {
            projectiles[i].update();
            bool hit = false;
            for (auto &z : zombies)
            {
                if (z.checkCollision(projectiles[i].x - 20, projectiles[i].y - 20, 40, 40))
                {
                    bloods.push_back(BloodStain(z.x, z.y, z.w, z.h));
                    z.respawn(camX, camY);
                    hit = true;
                    break;
                }
            }
            if (hit || projectiles[i].expired)
            {
                projectiles.erase(projectiles.begin() + i);
                i--;
            }
        }

        for (int i = 0; i < (int)particles.size(); i++)
        {
            particles[i].update();
            if (particles[i].expired)
            {
                particles.erase(particles.begin() + i);
                i--;
            }
        }

        for (auto &z : zombies)
        {
            z.update();
            if (inCar && z.checkCollision(myCar.x, myCar.y, myCar.w, myCar.h))
            {
                bloods.push_back(BloodStain(z.x, z.y, z.w, z.h));
                z.respawn(camX, camY);
            }
        }

        if (!inCar && player.alive)
        {
            for (auto &z : zombies)
                if (sqrt(pow(z.x - player.x, 2) + pow(z.y - player.y, 2)) < 80)
                    player.takeDamage();
        }
        if (!player.alive)
        {
            player.health = 1.0f;
            player.alive = true;
            inCar = true;
        }

        for (int i = 0; i < (int)bloods.size(); i++)
        {
            bloods[i].update();
            if (bloods[i].expired)
            {
                bloods.erase(bloods.begin() + i);
                i--;
            }
        }

        SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
        SDL_RenderClear(renderer);

        float dW = 1200;
        int sX = (int)floor(camX / dW) * dW, sY = (int)floor(camY / dW) * dW;
        for (int x = sX - (int)dW; x <= sX + SW + (int)dW; x += dW)
            for (int y = sY - (int)dW; y <= sY + SH + (int)dW; y += dW)
            {
                SDL_FRect r = {(float)x - camX, (float)y - camY, dW, dW};
                SDL_RenderCopyF(renderer, bgTex, NULL, &r);
            }

        for (auto &b : bloods)
            b.draw(renderer, bloodTex, camX, camY);
        for (auto &p : particles)
            p.draw(renderer, camX, camY);
        for (auto &pr : projectiles)
            pr.draw(renderer, camX, camY, aimJoy.angle);
        for (auto &z : zombies)
            z.draw(renderer, zombieFrames, camX, camY);

        myCar.draw(renderer, camX, camY, inCar && aimJoy.isTouching, aimJoy.angle, aimJoy.powerRatio, arrowTex);

        if (!inCar)
        {
            player.draw(renderer, camX, camY, moveJoy.isTouching);
            player.drawHealthBar(renderer);
        }

        SDL_SetRenderDrawColor(renderer, 100, 100, 100, 150);
        SDL_RenderFillRect(renderer, &btn);
        moveJoy.draw(renderer);
        if (inCar)
            aimJoy.draw(renderer);

        SDL_RenderPresent(renderer);
    }

    IMG_Quit();
    SDL_Quit();
    return 0;
}