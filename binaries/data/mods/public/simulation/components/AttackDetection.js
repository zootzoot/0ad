function AttackDetection() {}

AttackDetection.prototype.Schema =
	"<a:help>Detects incoming attacks.</a:help>" +
	"<a:example/>" +
	"<a:component type='system'/>" +
	"<empty/>";

AttackDetection.prototype.Init = function()
{
	this.suppressionTransferRange = 50; // Any attacks within this range will replace the previous attack suppression.
	this.suppressionRange = 150; // Other attacks within this distance will not be registered.
	this.suppressionTime = 40 * 1000; // Other attacks within this time will not be registered.

	this.suppressedList = [];
};

// Utility function for calculating the distance between two attack events.
AttackDetection.prototype.Distance = function(pos1, pos2)
{
	var xs = pos2.x - pos1.x;
	var zs = pos2.z - pos1.z;
	return Math.sqrt(Math.pow(xs, 2) + Math.pow(zs, 2));
};

AttackDetection.prototype.AddSuppression = function(event)
{
	this.suppressedList.push(event);

	var cmpTimer = Engine.QueryInterface(SYSTEM_ENTITY, IID_Timer);
	cmpTimer.SetTimeout(this.entity, IID_AttackDetection, "HandleTimeout", this.suppressionTime);
};

//// Message handlers ////

AttackDetection.prototype.OnGlobalAttacked = function(msg)
{
	var cmpPlayer = Engine.QueryInterface(this.entity, IID_Player);
	var cmpOwnership = Engine.QueryInterface(msg.target, IID_Ownership);
	if (cmpOwnership.GetOwner() == cmpPlayer.GetPlayerID())
		Engine.PostMessage(msg.target, MT_PingOwner);
};

//// External interface ////

AttackDetection.prototype.AttackAlert = function(target, attacker)
{
	var cmpPlayer = Engine.QueryInterface(this.entity, IID_Player);
	var cmpTargetOwnership = Engine.QueryInterface(target, IID_Ownership);
	// Don't register attacks dealt against other players.
	if (cmpTargetOwnership.GetOwner() != cmpPlayer.GetPlayerID())
		return;
	var cmpAttackerOwnership = Engine.QueryInterface(attacker, IID_Ownership);
	// Don't register attacks dealt by myself.
	if (cmpAttackerOwnership.GetOwner() == cmpPlayer.GetPlayerID())
		return;

	var cmpPosition = Engine.QueryInterface(target, IID_Position);
	if (!cmpPosition || !cmpPosition.IsInWorld())
		return;
	var cmpTimer = Engine.QueryInterface(SYSTEM_ENTITY, IID_Timer);
	var event = {target: target, position: cmpPosition.GetPosition(), time: cmpTimer.GetTime()};

	for (var i = 0; i < this.suppressedList.length; i++)
	{
		var element = this.suppressedList[i];
		
		// If the new attack is within suppression distance of this element then check if the element should be updated
		// and then return.
		var dist = this.Distance(element.position, event.position);
		if (dist < this.suppressionRange)
		{
			if (dist < this.suppressionTransferRange)
				element = event;
			return;
		}
	}
	
	this.AddSuppression(event);
	Engine.PostMessage(this.entity, MT_AttackDetected, { "player": cmpPlayer.GetPlayerID(), "event": event });
	var cmpGuiInterface = Engine.QueryInterface(SYSTEM_ENTITY, IID_GuiInterface);
	cmpGuiInterface.PushNotification({"type": "attack", "player": cmpPlayer.GetPlayerID(), "message": event});
};

AttackDetection.prototype.GetSuppressionTime = function()
{
	return this.suppressionTime;
};

AttackDetection.prototype.HandleTimeout = function()
{
	var cmpTimer = Engine.QueryInterface(SYSTEM_ENTITY, IID_Timer);
	var now = cmpTimer.GetTime();
	for (var i = 0; i < this.suppressedList.length; i++)
	{
		var event = this.suppressedList[i];
		
		// Check if this event has timed out.
		if (now - event.time >= this.suppressionTime)
		{
			this.suppressedList.splice(i, 1);
			i--;
			return;
		}
	}
};

AttackDetection.prototype.GetIncomingAttacks = function()
{
	return this.suppressedList;
};

Engine.RegisterComponentType(IID_AttackDetection, "AttackDetection", AttackDetection);
