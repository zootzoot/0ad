function BattleDetection() {}

BattleDetection.prototype.Schema =
	"<a:help>Detects the occurence of battles.</a:help>" +
	"<a:example/>" +
	"<a:component type='system'/>" +
	"<empty/>";

BattleDetection.prototype.Init = function()
{
	this.interval = 5 * 1000; // Interval over which damage rate should calculated in milliseconds.
	this.damageRateThreshold = 0.04; // Damage rate at which alertness is increased.
	this.alertnessMax = 4; // Maximum alertness level.
	this.alertnessThreshold = 2; // Alertness at which the player is considered in battle.

	this.pastAttacks = [];
	this.alertness = 0;

	this.StartTimer(0, this.interval);
};

BattleDetection.prototype.OnGlobalAttacked = function(msg)
{
	var cmpPlayer = Engine.QueryInterface(this.entity, IID_Player);
	// Only register attacks dealt by myself.
	var cmpAttackerOwnership = Engine.QueryInterface(msg.attacker, IID_Ownership);
	if (!cmpAttackerOwnership || cmpAttackerOwnership.GetOwner() != cmpPlayer.GetPlayerID())
		return;
	// Don't register attacks dealt against Gaia or invalid player.	
	var cmpTargetOwnership = Engine.QueryInterface(msg.target, IID_Ownership);
        if (!cmpTargetOwnership || cmpTargetOwnership.GetOwner() <= 0)
		return;

	var cmpTimer = Engine.QueryInterface(SYSTEM_ENTITY, IID_Timer);
	var currentTime = cmpTimer.GetTime();

	if (msg.damage)
		this.pastAttacks.push({time: currentTime, damage: msg.damage});

	this.updateDamageRate();
};

BattleDetection.prototype.updateDamageRate = function()
{
        var cmpTimer = Engine.QueryInterface(SYSTEM_ENTITY, IID_Timer);
        var currentTime = cmpTimer.GetTime();

	// Cull past attacks older than this.interval.
	var totalDamage = 0;
	var deltaTime = 0;
	for (var i = this.pastAttacks.length-1; i >= 0; i--)
	{
		deltaTime = currentTime - this.pastAttacks[i].time;
		if (deltaTime < this.interval)
			totalDamage += this.pastAttacks[i].damage;
		else
			this.pastAttacks.splice(i, 1);
	}

	// Calculate damage rate.
	this.damageRate = totalDamage / this.interval;
	return this.damageRate;
};

BattleDetection.prototype.updateAlertness = function()
{
	if (this.updateDamageRate() > this.damageRateThreshold)
		this.alertness = Math.min(this.alertnessMax, this.alertness+1);
	else
		this.alertness = Math.max(0, this.alertness-1);

	var cmpPlayer = Engine.QueryInterface(this.entity, IID_Player);
	if (this.alertness >= this.alertnessThreshold)
		warn("Player "+String(cmpPlayer.GetPlayerID())+" is in battle!");
	else
		warn("Player "+String(cmpPlayer.GetPlayerID())+" is at peace.");
};

BattleDetection.prototype.TimerHandler = function(data, lateness)
{
	// Reset the timer
	if (data.timerRepeat === undefined)
	{
		this.timer = undefined;
	}

	this.updateAlertness();
};

/**
 * Set up the BattleDetection timer to run after 'offset' msecs, and then
 * every 'repeat' msecs until StopTimer is called. A "Timer" message
 * will be sent each time the timer runs.
 */
BattleDetection.prototype.StartTimer = function(offset, repeat)
{
	if (this.timer)
		error("Called StartTimer when there's already an active timer");

	var data = { "timerRepeat": repeat };

	var cmpTimer = Engine.QueryInterface(SYSTEM_ENTITY, IID_Timer);
	if (repeat === undefined)
		this.timer = cmpTimer.SetTimeout(this.entity, IID_BattleDetection, "TimerHandler", offset, data);
	else
		this.timer = cmpTimer.SetInterval(this.entity, IID_BattleDetection, "TimerHandler", offset, repeat, data);
};

/**
 * Stop the current BattleDetection timer.
 */
BattleDetection.prototype.StopTimer = function()
{
	if (!this.timer)
		return;

	var cmpTimer = Engine.QueryInterface(SYSTEM_ENTITY, IID_Timer);
	cmpTimer.CancelTimer(this.timer);
	this.timer = undefined;
};

BattleDetection.prototype.OnGlobalAttacked = function(msg)
{
	var cmpPlayer = Engine.QueryInterface(this.entity, IID_Player);
	// Only register attacks dealt by myself.
	var cmpAttackerOwnership = Engine.QueryInterface(msg.attacker, IID_Ownership);
	if (cmpAttackerOwnership.GetOwner() != cmpPlayer.GetPlayerID())
		return;
	// Don't register attacks dealt against Gaia.	
	var cmpTargetOwnership = Engine.QueryInterface(msg.target, IID_Ownership);
	if (cmpTargetOwnership.GetOwner() == 0)
		return;

	var cmpTimer = Engine.QueryInterface(SYSTEM_ENTITY, IID_Timer);
	var currentTime = cmpTimer.GetTime();

	if (msg.damage)
		this.pastAttacks.push({time: currentTime, damage: msg.damage});

	this.updateDamageRate();
};

Engine.RegisterComponentType(IID_BattleDetection, "BattleDetection", BattleDetection);
