#include "BossA.h"
#include "../../Effect/effect.h"
#include "../../Player/player.h"
#include <cmath>

namespace BossA {

struct OniAttackBox {
	float x, y, width, height;
};

OniAttackBox GetOniAttackBox(const Boss& boss);
OniAttackBox GetOniTelegraphBox(const Boss& boss);
void CounterOniAttack(GameState& state);

namespace {

float Clamp(float value, float minimum, float maximum) {
	if (value < minimum)
		return minimum;
	if (value > maximum)
		return maximum;
	return value;
}

int GetWindup(OniAttack attack) {
	switch (attack) {
	case OniAttack::Smash:
		return kSmashWindup;
	case OniAttack::Sweep:
		return kSweepWindup;
	case OniAttack::Charge:
		return kChargeWindup;
	}
	return kSmashWindup;
}

int GetActive(OniAttack attack) {
	switch (attack) {
	case OniAttack::Smash:
		return kSmashActive;
	case OniAttack::Sweep:
		return kSweepActive;
	case OniAttack::Charge:
		return kChargeActive;
	}
	return kSmashActive;
}

int GetRecovery(OniAttack attack) {
	switch (attack) {
	case OniAttack::Smash:
		return kSmashRecovery;
	case OniAttack::Sweep:
		return kSweepRecovery;
	case OniAttack::Charge:
		return kChargeRecovery;
	}
	return kSmashRecovery;
}

void StartOniAttack(Boss& boss, const Player& player) {
	const float bossCenter = boss.x + kOniWidth * 0.5f;
	const float playerCenter = player.x + 18.0f;
	boss.direction = playerCenter < bossCenter ? -1 : 1;

	// 3種類を順番に出す。ランダムより初見で覚えやすい。
	switch (boss.attackCount % 3) {
	case 0:
		boss.attack = OniAttack::Smash;
		break;
	case 1:
		boss.attack = OniAttack::Sweep;
		break;
	default:
		boss.attack = OniAttack::Charge;
		break;
	}
	++boss.attackCount;
	boss.state = OniState::Telegraph;
	boss.stateTimer = GetWindup(boss.attack);
	boss.hasHit = false;
}

void HitPlayer(GameState& state) {
	if (state.playerInvincibleTimer > 0 || state.dashTimer > 0) {
		return;
	}
	--state.playerHp;
	state.playerInvincibleTimer = kPlayerInvincibleFrames;
	state.shakeTimer = 14;
	state.shakeTotalFrames = 14;
	state.shakeMaxPower = 13.0f;
	// 吹き飛ばし後も画面外へ出ない。
	state.player.x += state.boss.direction * 48.0f;
	state.player.x = Clamp(state.player.x, 0.0f, kWidth - 36.0f);
}

void TryOniAttackHit(GameState& state) {
	Boss& boss = state.boss;
	if (boss.hasHit || boss.state != OniState::Attack) {
		return;
	}
	const OniAttackBox area = GetOniAttackBox(boss);
	if (!IsHit(area.x, area.y, area.width, area.height, state.player.x, state.player.y, 36.0f, 48.0f)) {
		return;
	}

	// 当たり判定中に右クリックのカウンター受付が重なれば成功。
	if (state.justWindow) {
		CounterOniAttack(state);
		return;
	}

	// ダッシュ中は回避成功。判定は1回だけにする。
	boss.hasHit = true;
	HitPlayer(state);
}

} // namespace

void ResetBossBattle(Player& player, Boss& boss, Bullet bullets[]) {
	player = {260.0f, kGroundY - 48.0f, 1, 0.0f, true};
	boss = {};
	boss.x = 1120.0f;
	boss.y = kGroundY - kOniHeight;
	boss.direction = -1;
	boss.hp = kOniMaxHp;
	boss.state = OniState::Idle;
	boss.stateTimer = 30;

	// 旧BossAの弾を完全に消す。鬼は飛び道具を撃たない。
	for (int i = 0; i < kMaxBullets; ++i) {
		bullets[i] = {};
	}
}

OniAttackBox GetOniAttackBox(const Boss& boss) {
	const float front = boss.direction == 1 ? boss.x + kOniWidth - 25.0f : boss.x + 25.0f;
	if (boss.attack == OniAttack::Smash) {
		return {boss.direction == 1 ? front : front - 148.0f, kGroundY - 115.0f, 148.0f, 115.0f};
	}
	if (boss.attack == OniAttack::Sweep) {
		return {boss.direction == 1 ? front : front - 235.0f, kGroundY - 100.0f, 235.0f, 96.0f};
	}
	// 突進中は鬼の身体が当たり判定。
	return {boss.x + 10.0f, boss.y + 24.0f, kOniWidth - 20.0f, kOniHeight - 24.0f};
}

OniAttackBox GetOniTelegraphBox(const Boss& boss) {
	if (boss.attack != OniAttack::Charge) {
		return GetOniAttackBox(boss);
	}
	// 突進だけは実際の移動経路を予告する。
	const float pathLength = kOniChargeSpeed * kChargeActive;
	return {boss.direction == 1 ? boss.x : boss.x - pathLength, kGroundY - 140.0f, pathLength + kOniWidth, 140.0f};
}

void CounterOniAttack(GameState& state) {
	Boss& boss = state.boss;
	if (boss.state != OniState::Attack)
		return;

	boss.hasHit = true;
	boss.hp -= 2; // 右クリック成功は通常攻撃より強力
	if (boss.hp < 0)
		boss.hp = 0;
	boss.state = boss.hp > 0 ? OniState::Recovery : OniState::Dead;
	boss.stateTimer = 90;
	state.bossFlash = 18;
	state.counterWindowTimer = 0;
	state.counterArmed = false;
	state.justWindow = false;
	state.counterCooldown = kCounterRetryCooldown;

	const bool enhanced = state.dashTimer > 0;
	state.counterEffect = {state.player.x + 18.0f, state.player.y + 24.0f, 30, enhanced};
	state.shakeTimer = enhanced ? kDashCounterShakeFrames : kCounterShakeFrames;
	state.shakeTotalFrames = state.shakeTimer;
	state.shakeMaxPower = enhanced ? kDashCounterShakePower : kCounterShakePower;
	state.hitStopTimer = enhanced ? kDashCounterHitStopFrames : kCounterHitStopFrames;
	state.slowTimer = enhanced ? kDashCounterSlowFrames : kCounterSlowFrames;
	state.gameFrameAccumulator = 0.0f;
	if (enhanced)
		state.specialFlashTimer = kDashCounterFlashFrames;
}

void UpdateBoss(GameState& state) {
	Boss& boss = state.boss;
	if (boss.hp <= 0) {
		boss.state = OniState::Dead;
		return;
	}

	const float bossCenter = boss.x + kOniWidth * 0.5f;
	const float playerCenter = state.player.x + 18.0f;
	const float distance = fabsf(playerCenter - bossCenter);

	switch (boss.state) {
	case OniState::Idle:
		if (boss.stateTimer > 0) {
			--boss.stateTimer;
		} else if (distance > 177.0f) {
			boss.state = OniState::Move;
		} else {
			StartOniAttack(boss, state.player);
		}
		break;

	case OniState::Move:
		boss.direction = playerCenter < bossCenter ? -1 : 1;
		if (distance > 177.0f) {
			boss.x += boss.direction * kOniWalkSpeed;
			boss.x = Clamp(boss.x, 0.0f, kWidth - kOniWidth);
		} else {
			StartOniAttack(boss, state.player);
		}
		break;

	case OniState::Telegraph:
		// 予兆の間は方向を固定。プレイヤーは裏に回って避けられる。
		--boss.stateTimer;
		if (boss.stateTimer <= 0) {
			boss.state = OniState::Attack;
			boss.stateTimer = GetActive(boss.attack);
		}
		break;

	case OniState::Attack:
		if (boss.attack == OniAttack::Charge) {
			boss.x += boss.direction * kOniChargeSpeed;
			boss.x = Clamp(boss.x, 0.0f, kWidth - kOniWidth);
		}
		TryOniAttackHit(state);
		if (boss.state != OniState::Attack)
			break; // カウンターで中断
		--boss.stateTimer;
		if (boss.stateTimer <= 0) {
			boss.state = OniState::Recovery;
			boss.stateTimer = GetRecovery(boss.attack);
		}
		break;

	case OniState::Recovery:
		--boss.stateTimer;
		if (boss.stateTimer <= 0) {
			boss.state = OniState::Idle;
			boss.stateTimer = 16; // 攻撃間の小さな間
		}
		break;

	case OniState::Dead:
		break;
	}
}

void Reset(GameState& state) {
	state = {};
	state.playerHp = kPlayerMaxHp;
	state.playerInvincibleTimer = 0;
	ResetBossBattle(state.player, state.boss, state.bullets);
	state.shakeTotalFrames = kCounterShakeFrames;
	state.shakeMaxPower = kCounterShakePower;
}

// 鬼専用：プレイヤーの剣が命中したか判定する。
// 剣の攻撃判定
void CheckSwordHitBoss(GameState& state) {

	if (state.swordHitBoss) {
		return;
	}

	bool swordHitActive = false;

	if (state.normalSwordActive && state.swordSwingTimer <= kSwordHitStart && state.swordSwingTimer >= kSwordHitEnd) {
		swordHitActive = true;
	}

	if (swordHitActive == false) {
		return;
	}

	float hitX = state.player.direction == 1 ? state.player.x + 18.0f : state.player.x + 18.0f - kSwordHitWidth;

	float hitY = state.player.y + 24.0f - kSwordHitHeight * 0.5f;

	if (IsHit(hitX, hitY, kSwordHitWidth, kSwordHitHeight, state.boss.x, state.boss.y, kOniWidth, kOniHeight)) {

		state.swordHitBoss = true;
		--state.boss.hp; // 左クリック攻撃は1ダメージ
		if (state.boss.hp <= 0) {
			state.boss.hp = 0;
			state.boss.state = OniState::Dead;
		}
		state.swordHitFlashTimer = 12;
		state.bossFlash = 12;
	}
}

// BossA更新：攻撃入力／カウンター／AI／勝敗判定。
void Update(GameState& state, Scene& currentScene, const char keys[], const char preKeys[], bool leftMousePressed, bool rightMousePressed) {
	UpdateEffectTimers(state);

	// 右クリックを押した瞬間にカウンターの受付を開始。
	// 弾の反射ではなく、鬼の攻撃が当たる瞬間を受け止める。
	if (rightMousePressed && state.counterCooldown == 0) {
		state.counterArmed = true;
		state.counterWindowTimer = kJustCounterWindow;
		state.counterCooldown = kCounterRetryCooldown;
	}
	state.justWindow = state.counterArmed && state.counterWindowTimer > 0;

	// 左クリックで通常攻撃（1回のクリックで1回斬る）
	if (leftMousePressed && state.swordSwingTimer == 0) {
		state.swordRequest = true;
		state.normalSwordRequest = true;
	}

	if (keys[DIK_LSHIFT] != 0 && preKeys[DIK_LSHIFT] == 0) {
		state.dashRequest = true;
	}
	if (keys[DIK_SPACE] != 0 && preKeys[DIK_SPACE] == 0) {
		state.jumpRequest = true;
	}

	float gameSpeed = 1.0f;
	if (state.hitStopTimer > 0) {
		gameSpeed = 0.0f;
		--state.hitStopTimer;
	} else if (state.slowTimer > 0) {
		gameSpeed = kCounterSlowScale;
		--state.slowTimer;
	}

	state.gameFrameAccumulator += gameSpeed;
	if (state.gameFrameAccumulator >= 1.0f) {
		state.gameFrameAccumulator -= 1.0f;
		UpdatePlayer(state, keys, preKeys);

		// 被弾後の無敵時間を、ゲームが進むたびに1フレームずつ減らす。
		// 以前は増えるだけで減らず、一度被弾すると永久に無敵になっていた。
		if (state.playerInvincibleTimer > 0) {
			--state.playerInvincibleTimer;
		}

		BossA::CheckSwordHitBoss(state);
		UpdateBoss(state);
	}

	// 成功判定に使った「現在フレーム」を含めて12F受付。
	if (state.counterWindowTimer > 0) {
		--state.counterWindowTimer;
		if (state.counterWindowTimer <= 0) {
			state.counterArmed = false;
			state.justWindow = false;
		}
	}

	if (state.boss.hp <= 0) {
		currentScene = SCENE_CLEAR;
	} else if (state.playerHp <= 0) {
		currentScene = SCENE_GAMEOVER;
	}
}

// BossA描画：予兆、鬼、プレイヤー、HUD。
static void DrawOniWarning(const Boss& boss, int sx, int sy) {
	if (boss.state != OniState::Telegraph && boss.state != OniState::Attack) {
		return;
	}

	const bool preparing = boss.state == OniState::Telegraph;
	const OniAttackBox area = preparing ? GetOniTelegraphBox(boss) : GetOniAttackBox(boss);

	unsigned int color = 0xFF8A48FFu; // 振り下ろし：橙
	const char* label = "OVERHEAD!";
	if (boss.attack == OniAttack::Sweep) {
		color = 0xE758E5FFu; // 薙ぎ払い：紫
		label = "SWEEP!";
	} else if (boss.attack == OniAttack::Charge) {
		color = 0xFF4B4BFFu; // 突進：赤
		label = "CHARGE!";
	}

	const unsigned int alpha = preparing ? 48 : 105;
	Novice::DrawBox((int)area.x + sx, (int)area.y + sy, (int)area.width, (int)area.height, 0.0f, WithAlpha(color, alpha), kFillModeSolid);
	Novice::DrawBox((int)area.x + sx, (int)area.y + sy, (int)area.width, (int)area.height, 0.0f, color, kFillModeWireFrame);

	if (preparing) {
		int total = kSmashWindup;
		if (boss.attack == OniAttack::Sweep)
			total = kSweepWindup;
		if (boss.attack == OniAttack::Charge)
			total = kChargeWindup;
		int remainingBar = (boss.stateTimer * 150) / total;
		if (remainingBar < 0)
			remainingBar = 0;

		const int cx = (int)(boss.x + kOniWidth * 0.5f) + sx;
		const int y = (int)boss.y - 80 + sy;
		Novice::ScreenPrintf(cx - 48, y, "%s", label);
		Novice::DrawBox(cx - 75, y + 23, 150, 10, 0.0f, 0x363A4DFFu, kFillModeSolid);
		Novice::DrawBox(cx - 75, y + 23, remainingBar, 10, 0.0f, color, kFillModeSolid);
	}
}

static void DrawOni(const Boss& boss, int sx, int sy, bool flashing) {
	int x = (int)boss.x + sx;
	int y = (int)boss.y + sy;
	const unsigned int bodyColor = flashing ? 0xFFFFFFFFu : 0xAF4A50FFu;
	const unsigned int faceColor = flashing ? 0xFFFFFFFFu : 0xD85A4FFFu;

	const bool crouch = boss.attack == OniAttack::Charge && (boss.state == OniState::Telegraph || boss.state == OniState::Attack);
	if (crouch)
		y += 12;

	// 脚・胴・頭を基本図形で描き、画像素材なしでも動作確認できる。
	Novice::DrawBox(x + 29, y + 120, 32, crouch ? 48 : 60, 0.0f, 0x553E54FFu, kFillModeSolid);
	Novice::DrawBox(x + 76, y + 120, 32, crouch ? 48 : 60, 0.0f, 0x553E54FFu, kFillModeSolid);
	Novice::DrawBox(x + 22, y + 58, 92, 86, 0.0f, bodyColor, kFillModeSolid);
	Novice::DrawEllipse(x + 68, y + 42, 37, 39, 0.0f, faceColor, kFillModeSolid);
	// 2本の角
	Novice::DrawLine(x + 47, y + 14, x + 30, y - 23, 0xF5E5BCFFu);
	Novice::DrawLine(x + 88, y + 14, x + 106, y - 23, 0xF5E5BCFFu);
	// 目
	Novice::DrawEllipse(x + 55, y + 39, 6, 5, 0.0f, 0xFFECB0FFu, kFillModeSolid);
	Novice::DrawEllipse(x + 81, y + 39, 6, 5, 0.0f, 0xFFECB0FFu, kFillModeSolid);

	// 金棒を攻撃ごとに違う構えで描画。
	const int d = boss.direction;
	int baseX = x + 68 + d * 36;
	int baseY = y + 90;
	int tipX = baseX + d * 56;
	int tipY = baseY + 72;

	if (boss.state == OniState::Telegraph) {
		if (boss.attack == OniAttack::Smash) {
			// 頭上に高く振り上げる
			baseY = y + 34;
			tipX = baseX + d * 15;
			tipY = y - 92;
		} else if (boss.attack == OniAttack::Sweep) {
			// 身体の横で水平に構える
			tipX = baseX - d * 114;
			tipY = baseY - 17;
		} else {
			// 突進準備：低く腰を落として金棒を引く
			tipX = baseX - d * 85;
			tipY = baseY + 22;
		}
	} else if (boss.state == OniState::Attack) {
		if (boss.attack == OniAttack::Smash) {
			tipX = baseX + d * 145;
			tipY = (int)kGroundY - 8 + sy;
		} else if (boss.attack == OniAttack::Sweep) {
			tipX = baseX + d * 188;
			tipY = baseY + 3;
		} else {
			tipX = baseX - d * 60;
			tipY = baseY + 24;
		}
	}

	// 太さのある金棒（線の多重描画）
	for (int offset = -4; offset <= 4; offset += 2) {
		Novice::DrawLine(baseX + offset, baseY, tipX + offset, tipY, 0x4E515BFFu);
	}
	Novice::DrawEllipse(tipX, tipY, 11, 11, 0.0f, 0x727985FFu, kFillModeSolid);

	if (boss.state == OniState::Recovery) {
		Novice::ScreenPrintf(x + 43, y - 50, "OPEN!");
	}
}

// BossA戦描画
void Draw(const GameState& state) {
	const int sx = state.shakeX;
	const int sy = state.shakeY;

	Novice::ScreenPrintf(30, 24, "BOSS A - ONI");
	Novice::ScreenPrintf(30, 50, "PLAYER HP : %d / %d", state.playerHp, kPlayerMaxHp);
	Novice::ScreenPrintf(30, 76, "ONI HP    : %d / %d", state.boss.hp, kOniMaxHp);

	// 地面
	Novice::DrawBox(0, (int)kGroundY + sy, kWidth, 6, 0.0f, 0x53627CFFu, kFillModeSolid);

	// ダッシュ残像とエフェクトは従来の描画を流用
	for (int i = 0; i < kDashAfterimageCount; ++i) {
		const DashAfterimage& a = state.dashImages[i];
		if (a.timer <= 0)
			continue;
		Novice::DrawBox((int)a.x + sx, (int)a.y + sy, 36, 48, 0.0f, WithAlpha(0x4BBFFF00u, a.timer * 9), kFillModeSolid);
	}
	if (state.dashWave.timer > 0) {
		const int elapsed = kDashWaveFrames - state.dashWave.timer;
		const int radius = 12 + elapsed * 7;
		Novice::DrawEllipse((int)state.dashWave.x + sx, (int)state.dashWave.y + sy, radius, radius, 0.0f, WithAlpha(0x7DDFFF00u, state.dashWave.timer * 190 / kDashWaveFrames), kFillModeWireFrame);
	}

	// 攻撃範囲を鬼より先に描画し、予兆をはっきり見せる。
	DrawOniWarning(state.boss, sx, sy);
	DrawOni(state.boss, sx, sy, state.bossFlash > 0);

	// 被弾無敵中のプレイヤーは点滅。
	if (state.playerInvincibleTimer == 0 || (state.playerInvincibleTimer / 5) % 2 == 0) {
		Novice::DrawBox((int)state.player.x + sx, (int)state.player.y + sy, 36, 48, 0.0f, 0x66BFFFFFu, kFillModeSolid);
	}
	DrawSword(state.player, state.swordSwingTimer, sx, sy);

	// 右クリックのカウンター受付を青い円で表示。
	if (state.justWindow) {
		Novice::DrawEllipse((int)state.player.x + 18 + sx, (int)state.player.y + 24 + sy, 54, 54, 0.0f, 0x88EEFFFFu, kFillModeWireFrame);
		Novice::ScreenPrintf(30, 150, "COUNTER ACTIVE");
	}
	DrawCounterEffect(state.counterEffect, sx, sy);

	if (state.swordHitFlashTimer > 0) {
		Novice::ScreenPrintf(30, 177, "ATTACK HIT!");
	}
	if (state.counterEffect.timer > 0) {
		Novice::ScreenPrintf(30, 205, "COUNTER SUCCESS!");
	}

	// 操作案内（地面より下に置く）
	Novice::ScreenPrintf(30, 674, "A/D: MOVE   SPACE: JUMP   SHIFT: DASH");
	Novice::ScreenPrintf(30, 704, "LEFT CLICK: ATTACK   RIGHT CLICK: COUNTER");
	Novice::ScreenPrintf(30, 734, "ORANGE: OVERHEAD  PURPLE: SWEEP  RED: CHARGE");
	Novice::ScreenPrintf(30, 764, "Watch the windup. Dodge or counter, then punish the recovery!");
	if (state.counterCooldown > 0) {
		Novice::ScreenPrintf(30, 798, "COUNTER COOLDOWN: %d", state.counterCooldown);
	} else {
		Novice::ScreenPrintf(30, 798, "COUNTER READY");
	}

	if (state.specialFlashTimer > 0) {
		Novice::DrawBox(0, 0, kWidth, kHeight, 0.0f, WithAlpha(0xD8FAFF00u, 150 * state.specialFlashTimer / kDashCounterFlashFrames), kFillModeSolid);
	}
}

} // namespace BossA
