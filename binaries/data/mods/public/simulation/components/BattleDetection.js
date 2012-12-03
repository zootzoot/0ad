function BattleDetection() {}

BattleDetection.prototype.Schema =
	"<a:help>Detects the occurence of battles.</a:help>" +
	"<a:example/>" +
	"<a:component type='system'/>" +
	"<empty/>";

BattleDetection.prototype.Init = function()
{
	this.pastAttacks = [];
	this.interval = 10 * 1000; // Interval over which damage rate should calculated in milliseconds.
	this.damageRateThreshold = 0.04; // Damage rate at which the player is considered in battle.
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

	if (deltaTime)
		var damageRate = totalDamage / deltaTime;
	if (damageRate && damageRate > this.damageRateThreshold)
		warn("Player "+String(cmpPlayer.GetPlayerID())+" is in battle! (rate:"+String(damageRate)+")");
	else
		warn("Player "+String(cmpPlayer.GetPlayerID())+" is at peace. (rate:"+String(damageRate)+")");
};

Engine.RegisterComponentType(IID_BattleDetection, "BattleDetection", BattleDetection);
