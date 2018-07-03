//---------------------------------------------------------
//	Pyon!
//
//		©2013 Yuichiro Nakada
//---------------------------------------------------------
//g++ pyon.cpp -o pyon -I ../SoftPixelEngine/sources/ -L. -lSoftPixelEngine
//LD_LIBRARY_PATH=. ./pyon

#include <SoftPixelEngine.hpp>
#include "font.hpp"
using namespace sp;

// パラメータ
#define SCREEN_WIDTH		800
#define SCREEN_HEIGHT		600
// 画像
#define RESOURCE_PATH		"./"
#define IMAGE_PLAYER		RESOURCE_PATH "char.png"
#define IMAGE_BACKGROUND	RESOURCE_PATH "bg.png"
#define IMAGE_MAP		RESOURCE_PATH "map2.png"

/* マップチップ */
#define MAPCHIP_HEIGHT		16		// マップチップの高さ（ピクセル）
#define MAPCHIP_WIDTH		16		// マップチップの幅（ピクセル）
#define MAPCHIP_HEIGHT_COUNT	SCREEN_HEIGHT/MAPCHIP_HEIGHT	// マップチップの縦の数
#define MAPCHIP_WIDTH_COUNT	SCREEN_WIDTH/MAPCHIP_WIDTH	// マップチップの横の数

/* === Global members === */
SoftPixelDevice* spDevice	= 0;
io::InputControl* spControl	= 0;
video::RenderSystem* spRenderer	= 0;
video::RenderContext* spContext	= 0;
audio::SoundDevice* spListener	= 0;
FontTex *font;

video::Texture *TexBg = 0;			// 背景
video::Texture *TexMap = 0;			// マップ

int frame;	// フレーム
int score;	// スコア
struct Sprite {
	int frame;
	int width, height;	// サイズ
	float x, y;
	float vx, vy;		// 移動速度
	int time;		// 存在時間
	int jumpPow;		// ジャンプ力
	int jumpAble;		// ジャンプ可
	video::Texture *tex;	// テクスチャー
};
Sprite player;				// プレイヤー
Sprite map[MAPCHIP_WIDTH_COUNT];	// マップ


void DrawSprite(video::Texture *tex, s32 PosX, s32 PosY, s32 num)
{
	static const s32 CLIP_SIZE = 32;	// チップの大きさ
	static const s32 CHAR_SIZE = 32;	// 表示の大きさ

	s32 ClipX = num % (tex->getSize().Width / CLIP_SIZE);
	s32 ClipY = num / (tex->getSize().Width / CLIP_SIZE);

	// Set the clipping rectangle
	dim::rect2df ClipRect(
		ClipX * CLIP_SIZE, ClipY * CLIP_SIZE,
		(ClipX + 1) * CLIP_SIZE, (ClipY + 1) * CLIP_SIZE
	);

	// Resize rectangle to range [0.0 - 1.0]
	ClipRect.Left   /= tex->getSize().Width;
	ClipRect.Top    /= tex->getSize().Height;
	ClipRect.Right  /= tex->getSize().Width;
	ClipRect.Bottom /= tex->getSize().Height;

	// Draw the 2d image with a clipping rect
	spRenderer->draw2DImage(
		tex,
//		dim::rect2di(PosX - CHAR_SIZE/2, PosY - CHAR_SIZE/2, CHAR_SIZE, CHAR_SIZE),
		dim::rect2di(PosX, PosY, CHAR_SIZE, CHAR_SIZE),
		ClipRect
	);
}

void InitDevice()
{
	// Create the graphics device
	spDevice    = createGraphicsDevice(
		video::RENDERER_AUTODETECT, dim::size2di(SCREEN_WIDTH, SCREEN_HEIGHT), 32, "Pyon!"
	);

	// Create input control and get render system
	spControl   = spDevice->getInputControl();
	spRenderer  = spDevice->getRenderSystem();
	spContext   = spDevice->getRenderContext();
	spListener  = spDevice->getSoundDevice();

	// Make the background white (use "255" or "video::color(255)" or "video::color(255, 255, 255)")
	// Normally pointless because we draw a background image but if you have no image you can use
	// this function to set the background color
//	spRenderer->setClearColor(255);
	spRenderer->setClearColor(0);

	// Update window title
	spDevice->setWindowTitle(
		spDevice->getWindowTitle() + " [ " + spRenderer->getVersion() + " ]"
	);

	// To be sure the programs runs on each PC with the same speed activate the frame rates
	spDevice->setFrameRate(100);
}

void LoadResources()
{
	// for fonts
	font = new FontTex(spRenderer, SCREEN_WIDTH, SCREEN_HEIGHT, RESOURCE_PATH);

	// Load background texture
	TexBg = spRenderer->loadTexture(IMAGE_BACKGROUND);

	// Load map texture
	//TexMap = spRenderer->loadTexture(Path + "mori640x480.png");
	TexMap = spRenderer->loadTexture(IMAGE_MAP);

	// Load map texture (WOLF_RPG_Editor2)
//	TexMap = spRenderer->loadTexture(Path + "[Base]BaseChip_pipo.png");
	// Set color key (255, 0, 255) with transparency 0
	//TexMap->setColorKey(video::color(255, 0, 255, 0));
	// Set wrap mode to clamp-to-edges so that ugly borders cannot occur
	TexMap->setWrapMode(video::TEXWRAP_CLAMP);

	// Load character texture
	player.tex = spRenderer->loadTexture(IMAGE_PLAYER);

	/*spListener->setMelodySpeed(2);
	spListener->playMelody("O3;T250;S10;E;S250;E;E;S10;C;S250;E;S750;G;O2;S0;G;");*/

//	audio::Sound* melody = spListener->loadSound(Path + "16d004-05-dong-9801st_marriage_proposal.mp3");
	audio::Sound* melody = spListener->loadSound("auchi.wav");
	//melody->setVolume(80);
	melody->setVolume(0.25f);
	melody->play();
	spListener->updateSounds();
}

static int nextScore = 0;	// 難易度
void Setting()
{
	int i;

	// マップの生成
	for (i=0; i<MAPCHIP_WIDTH_COUNT; i++) {
		map[i].x = i * 16;
		map[i].y = 224;
	}

	// プレイヤー設定
	player.frame = 24;
	player.x = 24;
	player.y = 192;
	player.jumpPow = -1; // ジャンプ力
	player.jumpAble = 1; // ジャンプ可

	score = 0;
	nextScore = 0;
}
#define MAX(a, b) ( ((a)>(b) ) ? (a) : (b) )
#define MIN(a, b) ( ((a)<(b) ) ? (a) : (b) )
void (*Scene)();
void GameOver();
void DrawScene()
{
	int i;
	static int floorLen = 0;	// 床の長さ
	static int holeMax = 8;		// 穴の最大長(1〜8)
	static int floorMax = 0;	// 床の最大長(8〜1)
	static int heightMax = 10;	// 最大高さ(5〜10)

	// 背景描画
	spRenderer->draw2DImage(
		TexBg,
		dim::rect2di(0, 0, 800, 600),
		dim::rect2df(0, 0, 1, 1)
	);

	// スコアの加算
	//frame++;
	score++;

	// 難易度調整
	if (score/4 > nextScore) {
		nextScore += 300;

		int lv = MIN(nextScore / 300, 7);	// Level: 0〜7
		holeMax = 1 + lv;			// 穴の最大長(1〜8)
		floorMax = 8 - lv;			// 床の最大長(8〜1)
		heightMax = 2 + lv;			// 最大高さ(5〜10)
	}

	// プレイヤー処理
	if (player.jumpAble) {
		if (spControl->keyHit(io::KEY_SPACE)) {
			// ジャンプ
			//player.jumpPow = 20;
			player.jumpPow = 16;
		}
		player.frame = (frame / 6) % 4;
		//if (player.frame >= 2) player.frame = (player.frame == 2) ? 0 : 2;
		if (player.frame >= 3) player.frame = 1;
		player.frame += 24;
	} else {
		// ジャンプ中
		player.frame = (frame / 4) % 3;
		player.frame += 6;
		//player.frame = 6;
	}
	// 衝突判定
	if (player.y > map[3].y - 20) {
		player.frame = 8;
		//game.end(scoreLabel.score, "スコア " + scoreLabel.score + "点");
		frame = 0;
		Scene = GameOver;
	} else if (player.jumpPow >= 0) {
		// 上昇
		player.y -= player.jumpPow;
		player.jumpPow--;
	} else {
		// 下降
		player.y -= player.jumpPow;
		player.jumpPow--;
		player.jumpAble = 0;
		if (map[3].y != 320 && player.y > map[3].y - 32) {
			// 着地
			player.y = map[3].y - 32;
			player.jumpAble = 1;
			player.jumpPow = 0;
		}
	}
	//printf("%d\n",player.frame);
	DrawSprite(player.tex, player.x, player.y, player.frame);

	// 地面の移動
	for (i=0; i<MAPCHIP_WIDTH_COUNT; i++) map[i].x -= 2;
	if (map[0].x == -16) {
		// 地面のシフト
		for (i=0; i<MAPCHIP_WIDTH_COUNT; i++) {
			map[i].x += 16;
			if (i < MAPCHIP_WIDTH_COUNT-1) map[i].y = map[i + 1].y;
		}
		// 前回と同じ高さの床(または穴)の追加
		if (floorLen > 0) {
			floorLen--;
			map[MAPCHIP_WIDTH_COUNT - 1].y = map[MAPCHIP_WIDTH_COUNT - 2].y;
		} else if (map[MAPCHIP_WIDTH_COUNT - 2].y == 320) {
			// 床の追加
			floorLen = MAX(1 + rand() % floorMax, floorMax-3);
			map[MAPCHIP_WIDTH_COUNT - 1].y = 320 - 16 * (5 + rand() % heightMax);
		} else {
			// 穴の追加
			floorLen = 1 + rand() % holeMax;
			map[MAPCHIP_WIDTH_COUNT - 1].y = 320;
		}
	}
	for (i=0; i<MAPCHIP_WIDTH_COUNT; i++) {
		//DrawSprite(TexMap, map[i].x, map[i].y, 0, 16, 16);
		if (map[i].y != 320) {
			spRenderer->draw2DImage(
				TexMap,
				dim::rect2di(map[i].x, map[i].y, 16, 224),/*map[i].y-80*/
				dim::rect2df(0, 0, 1, 1)
			);
		}
	}

	// スコア表示
	char msg[256];
	//sprintf(msg, "Pyon!  SCORE: %d", frame);
	sprintf(msg, "Pyon!  SCORE: %d", score);
	font->DrawString(0, 0, msg);
	//font->DrawString(0, 0, "Pyon!  SCORE: " + frame);
}
void Title()
{
	font->DrawStringCenter(SCREEN_HEIGHT/2, (char*)"Pyon!");
	font->DrawPString(100, 100, "GAME START");
	if (spControl->keyHit(io::KEY_RETURN)) {
		Setting();
		frame = 0;
		Scene = DrawScene;
	}
}
void GameOver()
{
	font->DrawStringCenter(SCREEN_HEIGHT/2, (char*)"Game Over");
	char msg[256];
	sprintf(msg, "SCORE: %d", score);
	font->DrawString(0, 0, msg);
	if (spControl->keyHit(io::KEY_RETURN)) {
		frame = 0;
		Scene = Title;
	}
}
int main()
{
	InitDevice();
	LoadResources();
	Scene = Title;

	// Loop until the user presses the ESC key
	while (spDevice->updateEvent() && !spControl->keyDown(io::KEY_ESCAPE)) {
		spRenderer->clearBuffers();

		spRenderer->beginDrawing2D();
		{
			//DrawScene();
			Scene();
			frame++;
		}
		spRenderer->endDrawing2D();

		spContext->flipBuffers();
	}

	// Delete all allocated memor (objects, textures etc.) and close the screen
	deleteDevice();

	return 0;
}
