#include "BossA.h"
#include "../../Effect/effect.h"
#include "../../Player/player.h"
#include <cmath>

namespace BossA {

// 鬼ボス専用パラメータ（友達のチュートリアル設定は変更しない）
constexpr float kOniWidth = 136.0f;
constexpr float kOniHeight = 180.0f;
constexpr int kOniMaxHp = 300;
constexpr int kPlayerMaxHp = 6;
constexpr int kPlayerInvincibleFrames = 70;
constexpr int kSmashWindup = 72;
constexpr int kSmashActive = 12;
constexpr int kSmashRecovery = 72;
constexpr int kSweepWindup = 60;
constexpr int kSweepActive = 16;
constexpr int kSweepRecovery = 66;
constexpr int kChargeWindup = 84;
constexpr int kChargeActive = 46;
constexpr int kChargeRecovery = 90;
constexpr float kOniWalkSpeed = 3.4f;
constexpr float kOniChargeSpeed = 11.5f;
constexpr int kOniCounterRetryCooldown = 32;
constexpr int kOniJustCounterWindow = 12;
constexpr int kDownFrames = 300;
constexpr int kRoarFrames = 150;
constexpr int kBreakCount = 3;
constexpr int kSwordDamage = 2;
constexpr int kDownMultiplier = 3;
constexpr int kChargeTurnFrames = 30;
constexpr int kEnragedChargeActive = 60;
constexpr float kEnragedChargeSpeed = 14.0f;
constexpr float kEnragedWalkSpeed = 4.2f;

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

int GetBaseWindup(OniAttack attack) {
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

int GetBaseActive(OniAttack attack) {
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

int GetBaseRecovery(OniAttack attack) {
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

int GetWindup(const Boss& boss) {
	if (boss.isEnraged && boss.attack == OniAttack::Charge && boss.chargeCount > 1)
		return kChargeTurnFrames;
	return GetBaseWindup(boss.attack) * (boss.isEnraged ? 75 : 100) / 100;
}
int GetActive(const Boss& boss) { return boss.isEnraged && boss.attack == OniAttack::Charge ? kEnragedChargeActive : GetBaseActive(boss.attack); }
int GetRecovery(const Boss& boss) {
	if (!boss.isEnraged)
		return GetBaseRecovery(boss.attack);
	switch (boss.attack) {
	case OniAttack::Smash:
		return 54;
	case OniAttack::Sweep:
		return 50;
	default:
		return 72;
	}
}
float GetChargeSpeed(const Boss& boss) { return boss.isEnraged ? kEnragedChargeSpeed : kOniChargeSpeed; }

// 形態移行を最優先。HP半分で止め、演出中の操作・攻撃を無効化。
void ApplyOniDamage(GameState& state, int damage) {
	Boss& boss = state.boss;
	if (boss.state == OniState::Roar || boss.state == OniState::Dead)
		return;
	boss.hp -= damage;
	if (!boss.isEnraged && boss.hp <= kOniMaxHp / 2) {
		boss.hp = kOniMaxHp / 2;
		boss.isEnraged = true;
		boss.state = OniState::Roar;
		boss.stateTimer = kRoarFrames;
		boss.justGuardCount = 0;
		boss.chargeCount = 0;
		boss.hasHit = true;
		boss.roarStartX = state.player.x;
		boss.roarStartY = state.player.y;
		boss.roarTargetX = state.player.x + 18.0f < boss.x + kOniWidth * 0.5f ? 0.0f : kWidth - 36.0f;

		// 鬼が端にいる場合も、再開時にプレイヤーと重ならないようにする。
		if (fabsf(boss.roarTargetX + 18.0f - (boss.x + kOniWidth * 0.5f)) < 220.0f)
			boss.roarTargetX = boss.roarTargetX == 0.0f ? kWidth - 36.0f : 0.0f;
		state.dashTimer = 0;
		state.dashRequest = state.jumpRequest = state.swordRequest = state.normalSwordRequest = false;
		state.swordSwingTimer = 0;
		state.normalSwordActive = false;
		state.counterArmed = state.justWindow = false;
		state.counterWindowTimer = state.counterCooldown = 0;
		state.hitStopTimer = state.slowTimer = 0;
		state.gameFrameAccumulator = 0.0f;
	} else if (boss.hp <= 0) {
		boss.hp = 0;
		boss.state = OniState::Dead;
	}
}

void UpdateRoar(GameState& state) {
	Boss& boss = state.boss;
	const int elapsed = kRoarFrames - boss.stateTimer;
	if (elapsed >= 18) {
		const float t = Clamp((elapsed - 18) / 24.0f, 0.0f, 1.0f);
		const float eased = 1.0f - (1.0f - t) * (1.0f - t);
		state.player.x = boss.roarStartX + (boss.roarTargetX - boss.roarStartX) * eased;
		state.player.y = Clamp(boss.roarStartY + (kGroundY - 48.0f - boss.roarStartY) * t - 65.0f * sinf(t * 3.14159265f), 0.0f, kGroundY - 48.0f);
		state.player.velocityY = 0.0f;
		state.player.onGround = t >= 1.0f;
	}
	if (elapsed == 42) {
		state.shakeTimer = state.shakeTotalFrames = 78;
		state.shakeMaxPower = 16.0f;
	}
	if (--boss.stateTimer <= 0) {
		boss.state = OniState::Idle;
		boss.stateTimer = 30;
		state.playerInvincibleTimer = 30;
		state.counterCooldown = 0;
	}
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
	boss.chargeCount = 1;
	boss.stateTimer = GetWindup(boss);
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

void ResetOniBattle(Player& player, Boss& boss, Bullet bullets[]) {
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
	const float endX = Clamp(boss.x + boss.direction * GetChargeSpeed(boss) * GetActive(boss), 0.0f, kWidth - kOniWidth);
	return {boss.direction == 1 ? boss.x : endX, boss.y + 24.0f, fabsf(endX - boss.x) + kOniWidth, kOniHeight - 24.0f};
}

void CounterOniAttack(GameState& state) {
	Boss& boss = state.boss;
	if (boss.state != OniState::Attack)
		return;

	boss.hasHit = true;
	++boss.justGuardCount;
	ApplyOniDamage(state, 5);
	if (boss.state == OniState::Roar || boss.state == OniState::Dead)
		return;
	const bool broken = boss.justGuardCount >= kBreakCount;
	if (broken) {
		boss.state = OniState::Down;
		boss.stateTimer = kDownFrames;
		boss.chargeCount = 0;
	} else if (!(boss.isEnraged && boss.attack == OniAttack::Charge)) {
		boss.state = OniState::Recovery;
		boss.stateTimer = 90;
	}
	// 怒り突進は1・2回目のJGで止めない。hasHitで同一突進の多重判定を防ぐ。
	state.bossFlash = 18;
	state.counterWindowTimer = 0;
	state.counterArmed = false;
	state.justWindow = false;
	state.counterCooldown = kOniCounterRetryCooldown;

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
			boss.x += boss.direction * (boss.isEnraged ? kEnragedWalkSpeed : kOniWalkSpeed);
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
			boss.stateTimer = GetActive(boss);
		}
		break;

	case OniState::Attack:
		if (boss.attack == OniAttack::Charge) {
			boss.x += boss.direction * GetChargeSpeed(boss);
			boss.x = Clamp(boss.x, 0.0f, kWidth - kOniWidth);
		}
		TryOniAttackHit(state);
		if (boss.state != OniState::Attack)
			break; // カウンターで中断
		--boss.stateTimer;
		if (boss.stateTimer <= 0 || (boss.attack == OniAttack::Charge && boss.isEnraged && (boss.x <= 0.0f || boss.x >= kWidth - kOniWidth))) {
			if (boss.isEnraged && boss.attack == OniAttack::Charge && boss.chargeCount < 3) {
				++boss.chargeCount;
				boss.direction *= -1;
				boss.state = OniState::Telegraph;
				boss.stateTimer = GetWindup(boss);
				boss.hasHit = false;
			} else {
				boss.state = OniState::Recovery;
				boss.stateTimer = GetRecovery(boss);
			}
		}
		break;

	case OniState::Recovery:
		--boss.stateTimer;
		if (boss.stateTimer <= 0) {
			boss.state = OniState::Idle;
			boss.stateTimer = 16; // 攻撃間の小さな間
		}
		break;

	case OniState::Down:
		if (--boss.stateTimer <= 0) {
			boss.justGuardCount = 0;
			boss.state = OniState::Idle;
			boss.stateTimer = 16;
		}
		break;
	case OniState::Roar:
		break;
	case OniState::Dead:
		break;
	}
}

void Reset(GameState& state) {
	state = {};
	state.playerHp = kPlayerMaxHp;
	state.playerInvincibleTimer = 0;
	ResetOniBattle(state.player, state.boss, state.bullets);
	state.shakeTotalFrames = kCounterShakeFrames;
	state.shakeMaxPower = kCounterShakePower;
}

// 鬼への命中後の処理。剣の攻撃範囲と有効時間はplayer.cppで共通管理。
void CheckSwordHitBoss(GameState& state) {
	if (!IsSwordHitTarget(state, state.boss.x, state.boss.y, kOniWidth, kOniHeight)) {
		return;
	}
	state.swordHitBoss = true;
	if (state.boss.state == OniState::Roar || state.boss.state == OniState::Dead)
		return;
	ApplyOniDamage(state, kSwordDamage * (state.boss.state == OniState::Down ? kDownMultiplier : 1));
	state.swordHitFlashTimer = 12;
	state.bossFlash = 12;
}

// BossA更新：攻撃入力／カウンター／AI／勝敗判定。
void Update(GameState& state, Scene& currentScene, const char keys[], const char preKeys[], bool leftMousePressed, bool rightMousePressed) {
	UpdateEffectTimers(state);
	if (state.boss.state == OniState::Roar) {
		UpdateRoar(state);
		return;
	}

	// 右クリックを押した瞬間にカウンターの受付を開始。
	// 弾の反射ではなく、鬼の攻撃が当たる瞬間を受け止める。
	if (rightMousePressed && state.counterCooldown == 0) {
		state.counterArmed = true;
		state.counterWindowTimer = kOniJustCounterWindow;
		state.counterCooldown = kOniCounterRetryCooldown;
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
		// 剣の攻撃判定は共通、命中後の処理はBossAに分離。
		UpdatePlayer(state, keys);

		// 被弾後の無敵時間を、ゲームが進むたびに1フレームずつ減らす。
		// 以前は増えるだけで減らず、一度被弾すると永久に無敵になっていた。
		if (state.playerInvincibleTimer > 0) {
			--state.playerInvincibleTimer;
		}

		BossA::CheckSwordHitBoss(state);
		if (state.boss.state != OniState::Roar)
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
		int total = GetWindup(boss);
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
	const unsigned int bodyColor = flashing ? 0xFFFFFFFFu : (boss.isEnraged ? 0xC52C32FFu : 0xAF4A50FFu);
	const unsigned int faceColor = flashing ? 0xFFFFFFFFu : (boss.isEnraged ? 0xEF4136FFu : 0xD85A4FFFu);

	const bool crouch = boss.attack == OniAttack::Charge && (boss.state == OniState::Telegraph || boss.state == OniState::Attack);
	if (crouch)
		y += 12;
	if (boss.state == OniState::Down)
		y += 38;
	if (boss.isEnraged) {
		Novice::DrawEllipse(x + 68, y + 85, 85, 108, 0.0f, 0xFF382533u, kFillModeSolid);
	}

	// 脚・胴・頭を基本図形で描き、画像素材なしでも動作確認できる。
	Novice::DrawBox(x + 29, y + 120, 32, boss.state == OniState::Down ? 22 : (crouch ? 48 : 60), 0.0f, 0x553E54FFu, kFillModeSolid);
	Novice::DrawBox(x + 76, y + 120, 32, boss.state == OniState::Down ? 22 : (crouch ? 48 : 60), 0.0f, 0x553E54FFu, kFillModeSolid);
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

	if (boss.state == OniState::Down) {
		Novice::ScreenPrintf(x + 38, y - 65, "DOWN!");
	} else if (boss.state == OniState::Recovery) {
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

	Novice::DrawBox(30, 100, 400, 14, 0.0f, 0x333333FFu, kFillModeSolid);
	Novice::DrawBox(30, 100, 400 * state.boss.hp / kOniMaxHp, 14, 0.0f, 0xC94444FFu, kFillModeSolid);
	for (int i = 0; i < kBreakCount; ++i) {
		Novice::DrawBox(30 + i * 42, 122, 36, 12, 0.0f, i < state.boss.justGuardCount ? 0xFFD060FFu : 0x555555FFu, kFillModeSolid);
	}
	if (state.boss.isEnraged)
		Novice::ScreenPrintf(460, 76, "ENRAGED");
	if (state.boss.state == OniState::Down)
		Novice::ScreenPrintf(170, 120, "DOWN: %.1fs / DAMAGE x3", state.boss.stateTimer / 60.0f);
	if (state.boss.isEnraged && state.boss.attack == OniAttack::Charge && (state.boss.state == OniState::Telegraph || state.boss.state == OniState::Attack))
		Novice::ScreenPrintf(460, 100, "CHARGE %d / 3", state.boss.chargeCount);

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

	if (state.boss.state == OniState::Roar) {
		const int elapsed = kRoarFrames - state.boss.stateTimer;
		const int cx = (int)state.boss.x + 68 + sx;
		const int cy = (int)state.boss.y + 85 + sy;
		if (elapsed < 120) {
			Novice::DrawEllipse(cx, cy, 94, 116, 0.0f, 0x66CCFF44u, kFillModeSolid);
			Novice::DrawEllipse(cx, cy, 94, 116, 0.0f, 0x99DDFFFFu, kFillModeWireFrame);
		}
		if (elapsed >= 18 && elapsed < 42) {
			const int radius = 90 + (elapsed - 18) * 34;
			Novice::DrawEllipse(cx, cy, radius, radius, 0.0f, 0xFFDDAAFFu, kFillModeWireFrame);
		}
		if (elapsed >= 42 && elapsed < 120) {
			for (int i = 0; i < 48; ++i) {
				const float a = i * 6.2831853f / 48.0f;
				const float inner = 150.0f + ((i * 37 + elapsed * 11) % 100);
				Novice::DrawLine(cx + (int)(cosf(a) * inner), cy + (int)(sinf(a) * inner), cx + (int)(cosf(a) * 2400), cy + (int)(sinf(a) * 2400), 0xFFB0A099u);
			}
			Novice::ScreenPrintf(kWidth / 2 - 80, 250, "ROAR!!");
		}
	}

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
