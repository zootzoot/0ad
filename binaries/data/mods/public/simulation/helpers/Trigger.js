// CONDITION PROTOTYPES

var ConditionPrototypes = new Array();

ConditionPrototypes["PlayerUnitAttacked"] = function(inputs) {
	this.Player = inputs["player"];
	this.HasBeenAttacked = false;
	this.Attacker = null;
};
ConditionPrototypes["PlayerUnitAttacked"].prototype.Evaluate = function() {
	if (this.HasBeenAttacked)
		return {attacker: this.Attacker};
	else
		return false;
};
ConditionPrototypes["PlayerUnitAttacked"].prototype.OnGlobalAttacked = function(msg) {
	var cmpTargetOwnership = Engine.QueryInterface(msg.target, IID_Ownership);
	if (cmpTargetOwnership.GetOwner() == this.Player) {
	        this.HasBeenAttacked = true;
		this.Attacker = msg.attacker;
	}
};

Engine.RegisterGlobal("ConditionPrototypes", ConditionPrototypes);

// EFFECT PROTOTYPES

var EffectPrototypes = new Array();

EffectPrototypes["DebugWarning"] = function(inputs) {};
EffectPrototypes["DebugWarning"].prototype.Execute = function(inputs) {
	warn(inputs.message);
}

Engine.RegisterGlobal("EffectPrototypes", EffectPrototypes);
