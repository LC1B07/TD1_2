#include "BossA.h"
#include "../../Effect/effect.h"
#include "../../Player/player.h"
#include <cmath>

namespace BossA {

// 鬼ボス専用パラメータ（友達のチュートリアル設定は変更しない）
constexpr float kOniWidth = 136.0f;
constexpr float kOniHeight = 180.0f;
constexpr float kClubLength = 150.0f; // 全状態で共通の金棒の長さ（中心線）
constexpr int kOniMaxHp = 21000;
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
constexpr int kClashFrames = 90;
constexpr int kClashRequiredClicks = 8;
constexpr float kClashFallSpeed = 22.0f; // 鍔迫り合い中の強制下降速度
constexpr int kDownFrames = 150;
constexpr int kRoarFrames = 150;

constexpr int kSwordDamage = 140;
constexpr int kDownDamage = 350;
constexpr int kClashWinDamage = 5; // 従来の鍔迫り合い勝利ダメージ
constexpr float kClashHandGap = 172.39448f;
constexpr int kDamagePopupFrames = 60;
constexpr int kChargeTurnFrames = 30;
constexpr int kEnragedChargeActive = 60;
constexpr float kEnragedChargeSpeed = 14.0f;
constexpr float kEnragedWalkSpeed = 4.2f;

struct OniAttackBox {
	float x, y, width, height;
};

OniAttackBox GetOniAttackBox(const Boss& boss);
OniAttackBox GetOniTelegraphBox(const Boss& boss);
void StartClash(GameState& state);

struct ClubPose {
	float x, y, tipX, tipY;
};
ClubPose GetClubPose(const Boss& boss);

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

// 線分と矩形の最短距離を利用した金棒のカプセル判定。
float PointBoxDistSq(float x, float y, const OniAttackBox& b) {
	const float dx = x - Clamp(x, b.x, b.x + b.width);
	const float dy = y - Clamp(y, b.y, b.y + b.height);
	return dx * dx + dy * dy;
}
float PointSegmentDistSq(float x, float y, const ClubPose& c) {
	const float dx = c.tipX - c.x, dy = c.tipY - c.y;
	const float lenSq = dx * dx + dy * dy;
	const float t = lenSq > 0.0f ? Clamp(((x - c.x) * dx + (y - c.y) * dy) / lenSq, 0.0f, 1.0f) : 0.0f;
	const float ex = x - (c.x + dx * t), ey = y - (c.y + dy * t);
	return ex * ex + ey * ey;
}
bool ClipAxis(float origin, float delta, float lo, float hi, float& enter, float& exit) {
	if (fabsf(delta) < 0.0001f)
		return origin >= lo && origin <= hi;
	float a = (lo - origin) / delta, b = (hi - origin) / delta;
	if (a > b) {
		float tmp = a;
		a = b;
		b = tmp;
	}
	if (a > enter)
		enter = a;
	if (b < exit)
		exit = b;
	return enter <= exit;
}
bool ClubHitsBox(const ClubPose& c, const OniAttackBox& b, float radius) {
	float enter = 0.0f, exit = 1.0f;
	if (ClipAxis(c.x, c.tipX - c.x, b.x, b.x + b.width, enter, exit) && ClipAxis(c.y, c.tipY - c.y, b.y, b.y + b.height, enter, exit))
		return true;
	const float r2 = radius * radius;
	if (PointBoxDistSq(c.x, c.y, b) <= r2 || PointBoxDistSq(c.tipX, c.tipY, b) <= r2)
		return true;
	for (int i = 0; i < 4; ++i) {
		if (PointSegmentDistSq(b.x + ((i & 1) ? b.width : 0.0f), b.y + ((i & 2) ? b.height : 0.0f), c) <= r2)
			return true;
	}
	return false;
}

constexpr float kClubRadius = 11.0f;
constexpr float kClashBackReach = 38.0f;
constexpr float kClashVerticalReach = 82.0f;

bool CanClashWithClub(const GameState& state, const ClubPose& club) {
	// 通常ダメージの命中済みフラグとは独立。密着時に先に本体を斬っても成立。
	if (!state.normalSwordActive || state.swordSwingTimer <= 0)
		return false;
	const float cx = state.player.x + 18.0f, cy = state.player.y + 24.0f;
	// 鬼の背後では成立させない。正面の密着時は補助判定を維持する。
	const float bossCenter = state.boss.x + kOniWidth * 0.5f;
	if ((cx - bossCenter) * state.boss.direction <= 0.0f)
		return false;
	// 双方が向き合っているときだけ成立。
	if (state.player.direction != -state.boss.direction)
		return false;
	const float forward = kSwordHitWidth;
	const float left = state.player.direction == 1 ? cx - kClashBackReach : cx - forward;
	const OniAttackBox reach = {left, cy - kClashVerticalReach, forward + kClashBackReach, kClashVerticalReach * 2.0f};
	return ClubHitsBox(club, reach, kClubRadius);
}

// 鍔迫り合い時の左右対称の固定配置。画面端では鬼も必要分だけ移動。
void AlignClash(GameState& state) {
	Boss& boss = state.boss;
	const int d = boss.direction;
	const float offset = 36.0f + kClashHandGap + 12.0f;
	const float minCenter = d == 1 ? 68.0f : offset + 18.0f;
	const float maxCenter = d == 1 ? kWidth - offset - 18.0f : kWidth - 68.0f;
	const float center = Clamp(boss.x + 68.0f, minCenter, maxCenter);
	boss.x = center - 68.0f;
	state.player.direction = -d;
	state.player.x = center + d * offset - 18.0f;

	// 金棒の手元から120px、剣の手元から60pxの交点を求める。
	// 両武器の長さは維持し、空中から着地する間も接触点を追従。
	const float dy = state.player.y + 24.0f - (boss.y + 105.0f);
	const float dx = kClashHandGap;
	const float distance = sqrtf(dx * dx + dy * dy);
	const float a = (120.0f * 120.0f - 60.0f * 60.0f + distance * distance) / (2.0f * distance);
	const float h = sqrtf(Clamp(120.0f * 120.0f - a * a, 0.0f, 14400.0f));
	const float contactX = (a * dx + h * dy) / distance;
	const float contactY = (a * dy - h * dx) / distance;
	boss.clashClubAngle = atan2f(contactY, contactX);
}

void AddDamagePopup(Boss& boss, int damage, float x, float y) {
	if (damage <= 0)
		return;
	OniDamagePopup& popup = boss.damagePopups[boss.damagePopupIndex];
	popup = {x, y - 24.0f, damage, kDamagePopupFrames};
	boss.damagePopupIndex = (boss.damagePopupIndex + 1) % 32;
}

void UpdateDamagePopups(Boss& boss) {
	for (auto& popup : boss.damagePopups) {
		if (popup.timer > 0) {
			--popup.timer;
			popup.y -= 0.8f;
		}
	}
}

// 形態移行を最優先。HP半分で止め、演出中の操作・攻撃を無効化。
void ApplyOniDamage(GameState& state, int damage, float hitX, float hitY) {
	Boss& boss = state.boss;
	if (boss.state == OniState::Roar || boss.state == OniState::Dead)
		return;
	const int oldHp = boss.hp;
	boss.hp -= damage;
	if (!boss.isEnraged && boss.hp <= kOniMaxHp / 2) {
		boss.hp = kOniMaxHp / 2;
		boss.isEnraged = true;
		boss.state = OniState::Roar;
		boss.stateTimer = kRoarFrames;
		boss.clashClicks = 0;
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
	AddDamagePopup(boss, oldHp - boss.hp, hitX, hitY);
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
	const OniAttackBox playerBox = {state.player.x, state.player.y, 36.0f, 48.0f};
	if (boss.attack == OniAttack::Charge) {
		// 突進は鍔迫り合い不可。剣を振っても止められない。
		const OniAttackBox area = GetOniAttackBox(boss);
		if (!IsHit(area.x, area.y, area.width, area.height, playerBox.x, playerBox.y, playerBox.width, playerBox.height))
			return;
	} else {
		const ClubPose club = GetClubPose(boss);
		// 金棒との鍔迫り合いを、プレイヤーの被弾より先に判定。
		if (CanClashWithClub(state, club)) {
			StartClash(state);
			return;
		}
		if (!ClubHitsBox(club, playerBox, kClubRadius))
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

// 描画と攻撃判定で共有する金棒の始点・終点（ワールド座標）。
ClubPose GetClubPose(const Boss& boss) {
	const int d = boss.direction;
	float y = boss.y;
	if (boss.attack == OniAttack::Charge && (boss.state == OniState::Telegraph || boss.state == OniState::Attack))
		y += 12.0f;
	if (boss.state == OniState::Down)
		y += 38.0f;
	const float x = boss.x + 68.0f + d * 36.0f;
	const float handY = y + (boss.attack == OniAttack::Sweep && boss.state != OniState::Clash ? 125.0f : 105.0f);
	const bool active = boss.state == OniState::Attack || boss.state == OniState::Clash;
	// 先端の半径も考慮し、長さを変えず角度だけで地面に接触させる。
	const float groundAngle = asinf(Clamp((kGroundY - kClubRadius - handY) / kClubLength, -1.0f, 1.0f));
	const float idleAngle = Clamp(0.25f, -1.57f, groundAngle);
	float angle = idleAngle;
	if (boss.attack != OniAttack::Charge) {
		const float startAngle = boss.attack == OniAttack::Smash ? -1.45f : -0.65f;
		const float endAngle = boss.attack == OniAttack::Smash ? groundAngle : Clamp(0.30f, -1.57f, groundAngle);
		if (active || boss.state == OniState::Telegraph) {
			const int timer = boss.state == OniState::Clash ? boss.clashClubTimer : boss.stateTimer;
			const float progress = active ? Clamp((GetActive(boss) - timer) / (float)(GetActive(boss) - 1), 0.0f, 1.0f) : 0.0f;
			// 振り下ろしは加速しながら地面へ、最後の数フレームは接地。
			const float t = boss.attack == OniAttack::Smash ? Clamp(progress / 0.82f, 0.0f, 1.0f) : progress;
			angle = startAngle + (endAngle - startAngle) * (boss.attack == OniAttack::Smash ? t * t : t);
		} else if (boss.state == OniState::Recovery) {
			const float elapsed = (float)(GetRecovery(boss) - boss.stateTimer);
			const float t = Clamp((elapsed - 10.0f) / 18.0f, 0.0f, 1.0f);
			angle = endAngle + (idleAngle - endAngle) * t;
		}
	} else if (active || boss.state == OniState::Telegraph) {
		angle = 2.8883f;
	}
	if (boss.state == OniState::Clash)
		angle = boss.clashClubAngle;
	// 待機・ダウン・突進も含め、金棒の外周を地面より上に保つ。
	if (sinf(angle) > sinf(groundAngle)) {
		angle = cosf(angle) >= 0.0f ? groundAngle : 3.14159265f - groundAngle;
	}
	return {x, handY, x + d * cosf(angle) * kClubLength, handY + sinf(angle) * kClubLength};
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
		// 金棒が通る範囲の概略。実際の判定は金棒だけ。
		const float x = boss.x + 68.0f + boss.direction * 36.0f;
		const float handY = boss.y + (boss.attack == OniAttack::Sweep ? 125.0f : 105.0f);
		const float reach = kClubLength;
		const float top = handY - (boss.attack == OniAttack::Sweep ? 125.0f : 160.0f);
		return {boss.direction == 1 ? x - 11.0f : x - reach - 11.0f, top, reach + 22.0f, kGroundY - top};
	}
	// 突進だけは実際の移動経路を予告する。
	const float endX = Clamp(boss.x + boss.direction * GetChargeSpeed(boss) * GetActive(boss), 0.0f, kWidth - kOniWidth);
	return {boss.direction == 1 ? boss.x : endX, boss.y + 24.0f, fabsf(endX - boss.x) + kOniWidth, kOniHeight - 24.0f};
}

// 攻撃がぶつかったら、攻撃と移動を止めて連打フェーズへ。
void StartClash(GameState& state) {
	Boss& boss = state.boss;
	boss.clashClubTimer = boss.stateTimer;
	boss.state = OniState::Clash;
	boss.stateTimer = kClashFrames;
	boss.clashClicks = 0;
	boss.hasHit = true;
	// 武器が届く高さに収め、残りは従来通り地面へ押し下げる。
	state.player.y = Clamp(state.player.y, kGroundY - 145.0f, kGroundY - 48.0f);
	AlignClash(state);
	state.swordHitBoss = true;
	state.normalSwordActive = false;
	state.swordSwingTimer = 0;
	state.dashTimer = 0;
	state.player.velocityY = 0.0f;
	state.swordRequest = state.normalSwordRequest = state.dashRequest = state.jumpRequest = false;
	state.shakeTimer = state.shakeTotalFrames = 12;
	state.shakeMaxPower = 10.0f;
}

void UpdateClash(GameState& state, bool pressed) {
	Boss& boss = state.boss;
	// 空中で成立した場合も停止させず、地面まで押し下げる。
	const float floorY = kGroundY - 48.0f;
	state.player.y = Clamp(state.player.y + kClashFallSpeed, 0.0f, floorY);
	state.player.onGround = state.player.y >= floorY;
	state.player.velocityY = state.player.onGround ? 0.0f : kClashFallSpeed;
	AlignClash(state);
	if (pressed && boss.clashClicks < kClashRequiredClicks) {
		++boss.clashClicks;
		state.shakeTimer = state.shakeTotalFrames = 4;
		state.shakeMaxPower = 5.0f;
	}
	if (boss.clashClicks >= kClashRequiredClicks && state.player.onGround) {
		boss.state = OniState::Down;
		boss.stateTimer = kDownFrames;
		boss.chargeCount = 0;
		boss.clashClicks = 0;
		state.bossFlash = 18;
		state.shakeTimer = state.shakeTotalFrames = 16;
		state.shakeMaxPower = 18.0f;
		ApplyOniDamage(state, kClashWinDamage, boss.x + 68.0f, boss.y + 100.0f);
	} else if (boss.clashClicks < kClashRequiredClicks && --boss.stateTimer <= 0) {
		boss.clashClicks = 0;
		boss.chargeCount = 0;
		boss.state = OniState::Recovery;
		boss.stateTimer = GetRecovery(boss);
		HitPlayer(state);
	}
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
			TryOniAttackHit(state);
		}
		break;

	case OniState::Attack:
		--boss.stateTimer;
		if (boss.attack == OniAttack::Charge) {
			boss.x += boss.direction * GetChargeSpeed(boss);
			boss.x = Clamp(boss.x, 0.0f, kWidth - kOniWidth);
		}
		TryOniAttackHit(state);
		if (boss.state != OniState::Attack)
			break; // カウンターで中断
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
			boss.clashClicks = 0;
			boss.state = OniState::Idle;
			boss.stateTimer = 16;
		}
		break;
	case OniState::Clash:
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
	if (state.boss.state == OniState::Clash)
		return;
	if (!IsSwordHitTarget(state, state.boss.x, state.boss.y, kOniWidth, kOniHeight)) {
		return;
	}
	state.swordHitBoss = true;
	if (state.boss.state == OniState::Roar || state.boss.state == OniState::Dead)
		return;
	// 剣の攻撃矩形と鬼の矩形の重なりの中心を命中位置にする。
	const float left = state.player.x + 18.0f - (state.player.direction == -1 ? kSwordHitWidth : 0.0f);
	const float top = state.player.y + 24.0f - kSwordHitHeight * 0.5f;
	const float x1 = fmaxf(left, state.boss.x);
	const float x2 = fminf(left + kSwordHitWidth, state.boss.x + kOniWidth);
	const float y1 = fmaxf(top, state.boss.y);
	const float y2 = fminf(top + kSwordHitHeight, state.boss.y + kOniHeight);
	ApplyOniDamage(state, state.boss.state == OniState::Down ? kDownDamage : kSwordDamage, (x1 + x2) * 0.5f, (y1 + y2) * 0.5f);
	state.swordHitFlashTimer = 12;
	state.bossFlash = 12;
}

// BossA更新：攻撃入力／カウンター／AI／勝敗判定。
void Update(GameState& state, Scene& currentScene, const char keys[], const char preKeys[], bool leftMousePressed, bool rightMousePressed) {
	UpdateEffectTimers(state);
	UpdateDamagePopups(state.boss);
	if (state.boss.state == OniState::Roar) {
		UpdateRoar(state);
		return;
	}

	(void)rightMousePressed; // 鬼戦ではガード廃止
	if (state.boss.state == OniState::Clash) {
		UpdateClash(state, leftMousePressed);
		if (state.boss.hp <= 0)
			currentScene = SCENE_CLEAR;
		else if (state.playerHp <= 0)
			currentScene = SCENE_GAMEOVER;
		return;
	}

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

		// 鍔迫り合いを通常の命中より優先する。
		UpdateBoss(state);
		if (state.boss.state != OniState::Roar && state.boss.state != OniState::Clash)
			BossA::CheckSwordHitBoss(state);
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
	if (!preparing && boss.attack != OniAttack::Charge)
		return;
	const OniAttackBox area = preparing ? GetOniTelegraphBox(boss) : GetOniAttackBox(boss);

	unsigned int color = 0xFF8A48FFu; // 振り下ろし：橙
	const char* label = "OVERHEAD!";
	if (boss.attack == OniAttack::Sweep) {
		color = 0xE758E5FFu; // 薙ぎ払い：紫
		label = "SWEEP!";
	} else if (boss.attack == OniAttack::Charge) {
		color = 0xFF4B4BFFu; // 突進：赤
		label = "CHARGE! DODGE!";
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

	// 当たり判定と同じ座標から金棒を描画する。
	const ClubPose club = GetClubPose(boss);
	const float dx = club.tipX - club.x, dy = club.tipY - club.y;
	const float length = sqrtf(dx * dx + dy * dy);
	const float nx = length > 0.0f ? -dy / length : 0.0f;
	const float ny = length > 0.0f ? dx / length : 0.0f;
	for (int offset = -10; offset <= 10; ++offset) {
		Novice::DrawLine((int)(club.x + nx * offset) + sx, (int)(club.y + ny * offset) + sy, (int)(club.tipX + nx * offset) + sx, (int)(club.tipY + ny * offset) + sy, 0x4E515BFFu);
	}
	Novice::DrawEllipse((int)club.x + sx, (int)club.y + sy, 11, 11, 0.0f, 0x4E515BFFu, kFillModeSolid);
	Novice::DrawEllipse((int)club.tipX + sx, (int)club.tipY + sy, 11, 11, 0.0f, 0x727985FFu, kFillModeSolid);

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

	if (state.boss.isEnraged)
		Novice::ScreenPrintf(460, 76, "ENRAGED");
	if (state.boss.state == OniState::Down)
		Novice::ScreenPrintf(170, 120, "DOWN: %.1fs / DAMAGE 350", state.boss.stateTimer / 60.0f);
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
	if (state.boss.state != OniState::Clash)
		DrawSword(state.player, state.swordSwingTimer, sx, sy);

	if (state.boss.state == OniState::Clash) {
		// 金棒の途中と剣の途中が交差する専用姿勢。
		const ClubPose club = GetClubPose(state.boss);
		const float contactX = club.x + (club.tipX - club.x) * 0.8f;
		const float contactY = club.y + (club.tipY - club.y) * 0.8f;
		const float handX = state.player.x + 18.0f + state.player.direction * 12.0f;
		const float handY = state.player.y + 24.0f;
		const float dx = contactX - handX, dy = contactY - handY;
		const float length = sqrtf(dx * dx + dy * dy);
		const float ux = dx / length, uy = dy / length;
		const float tipX = handX + ux * kSwordLength, tipY = handY + uy * kSwordLength;
		Novice::DrawLine((int)handX + sx, (int)handY + sy, (int)tipX + sx, (int)tipY + sy, 0x52BFFFFFu);
		Novice::DrawLine((int)(handX - uy * 2) + sx, (int)(handY + ux * 2) + sy, (int)(tipX - uy * 2) + sx, (int)(tipY + ux * 2) + sy, 0xE5F9FFFFu);
		Novice::DrawLine((int)(handX - ux * 12) + sx, (int)(handY - uy * 12) + sy, (int)handX + sx, (int)handY + sy, 0x326E9BFFu);
		Novice::DrawLine((int)(handX - uy * 7) + sx, (int)(handY + ux * 7) + sy, (int)(handX + uy * 7) + sx, (int)(handY - ux * 7) + sy, 0xFFFFFFFFu);
		Novice::DrawEllipse((int)contactX + sx, (int)contactY + sy, 12, 12, 0.0f, 0xFFDC55FFu, kFillModeWireFrame);
		Novice::ScreenPrintf(kWidth / 2 - 100, 220, "CLASH! CLICK RAPIDLY!");
		Novice::DrawBox(kWidth / 2 - 150, 250, 300, 20, 0.0f, 0x333333FFu, kFillModeSolid);
		Novice::DrawBox(kWidth / 2 - 150, 250, 300 * state.boss.clashClicks / kClashRequiredClicks, 20, 0.0f, 0xFFD060FFu, kFillModeSolid);
		Novice::DrawBox(kWidth / 2 - 150, 280, 300 * state.boss.stateTimer / kClashFrames, 8, 0.0f, 0x77CFFFFFu, kFillModeSolid);
		Novice::ScreenPrintf(kWidth / 2 - 50, 300, "%d / %d", state.boss.clashClicks, kClashRequiredClicks);
	}

	if (state.swordHitFlashTimer > 0) {
		Novice::ScreenPrintf(30, 177, "ATTACK HIT!");
	}
	if (state.counterEffect.timer > 0) {
		Novice::ScreenPrintf(30, 205, "COUNTER SUCCESS!");
	}

	// 命中位置に固定された数値を上へ浮かせる。複数ヒットも個別表示。
	for (const auto& popup : state.boss.damagePopups) {
		if (popup.timer <= 0)
			continue;
		int digits = 1;
		for (int value = popup.damage; value >= 10; value /= 10)
			++digits;
		Novice::ScreenPrintf((int)Clamp(popup.x + sx - digits * 4.0f, 0.0f, kWidth - digits * 8.0f), (int)Clamp(popup.y + sy, 0.0f, kHeight - 16.0f), "%d", popup.damage);
	}

	// 操作案内（地面より下に置く）
	Novice::ScreenPrintf(30, 674, "A/D: MOVE   SPACE: JUMP   SHIFT: DASH");
	Novice::ScreenPrintf(30, 704, "LEFT CLICK: ATTACK / CLASH");
	Novice::ScreenPrintf(30, 734, "ORANGE: OVERHEAD  PURPLE: SWEEP  RED: CHARGE");
	Novice::ScreenPrintf(30, 764, "Clash with the club! Dodge red charges with movement / dash.");

	if (state.specialFlashTimer > 0) {
		Novice::DrawBox(0, 0, kWidth, kHeight, 0.0f, WithAlpha(0xD8FAFF00u, 150 * state.specialFlashTimer / kDashCounterFlashFrames), kFillModeSolid);
	}
}

} // namespace BossA
