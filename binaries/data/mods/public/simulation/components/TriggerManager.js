function Condition(condition_def, parent) {
	if (ConditionPrototypes[condition_def.Type]) {
		var condition = new ConditionPrototypes[condition_def.Type](parent.GetInputs(condition_def.Inputs));
		condition.Parent = parent;
		condition.Type = condition_def.Type;
		condition.InputDefs = condition_def.Inputs;
		condition.OutputDefs = condition_def.Outputs;
		return condition;
	} else {
		error("Unknown condition type: '"+condition_def.Type+"'.");
		return null;
	}
}

function Effect(effect_def, parent) {
	if (EffectPrototypes[effect_def.Type]) {
		var effect =  new EffectPrototypes[effect_def.Type](parent.GetInputs(effect_def.Inputs));
		effect.Parent = parent;
		effect.Type = effect_def.Type;
		effect.InputDefs = effect_def.Inputs;
		effect.OutputDefs = effect_def.Outputs;
		return effect;
	} else {
		error("Unknown effect type: '"+effect_def.Type+"'.");
		return null;
	}
}

function Trigger(trigger_def) {
	this.Uid = trigger_def.Uid;
	this.Outputs = {};
	this.Conditions = new Array();
	for (index in trigger_def.Conditions) {
		var condition = new Condition(trigger_def.Conditions[index], this);
		this.Conditions.push(condition);
	};
	this.Effects = new Array();
	for (index in trigger_def.Effects) {
		var effect = new Effect(trigger_def.Effects[index], this);
		this.Effects.push(effect);
	};
}

Trigger.prototype.HasFired = false;

Trigger.prototype.SetOutputs = function(values, output_defs) {
	for (x in output_defs) {
		var output_def = output_defs[x];
		if (this.Outputs[output_def.Name]) {
			error("Duplicate declaration of output '"+output_def.Name+"' in trigger #"+this.Uid+".");
			return false;
		} else {
			this.Outputs[output_def.Name] = values[output_def.Key];
		}
	}
	return true;
}

// Construct an object of input literals from an array of abstract input definitions for this trigger.
Trigger.prototype.GetInputs = function(input_defs)
{
		var inputs = {};
		for (m in input_defs) {
			var input_def = input_defs[m];
			if (input_def.Key)
				inputs[input_def.Name] = this.Outputs[input_def.Key];
			else if (input_def.Value)
				inputs[input_def.Name] = input_def.Value;
			else
				error("Invalid value in input '"+input_def.Name+"'.");
		}
		return inputs;
}

Trigger.prototype.Fire = function()
{
	for (n in this.Effects) {
		var effect = this.Effects[n];
		var inputs = this.GetInputs(effect.InputDefs);
		if (effect.Execute) {
			var values = effect.Execute(inputs);
			this.SetOutputs(values, effect.OutputDefs);
		} else if (effect.Type) {
			error("Failed to execute effect of type '"+effect.Type+"'.");
		}
	}
	this.HasFired = true;
};


function TriggerManager() {}

TriggerManager.prototype.Schema =
	"<a:component type='system'/><empty/>";

TriggerManager.prototype.Init = function()
{
	this.Triggers = new Array();
};

TriggerManager.prototype.AddTrigger = function(trigger_def)
{
	this.Triggers[trigger_def.Uid] = new Trigger(trigger_def);
};

TriggerManager.prototype.HandleEvent = function(event, msg)
{
	for (n in this.Triggers) {
		var trigger = this.Triggers[n];
		for (m in trigger.Conditions) {
			var condition = trigger.Conditions[m];
			if (condition[event])
				condition[event](msg);
		}
	}
};

TriggerManager.prototype.Evaluate = function()
{
	for (n in this.Triggers) {
		var trigger = this.Triggers[n];
		trigger.Outputs = {};
		var result = true;
		for (m in trigger.Conditions) {
			var condition = trigger.Conditions[m];
			var values = condition.Evaluate();
			trigger.SetOutputs(values, condition.OutputDefs)
			result = result && Boolean(values);
			if (result == false)
				break;
		}
		if (result && !trigger.HasFired)
			trigger.Fire();
	}
	return true;
};

TriggerManager.prototype.OnUpdate = function()
{
	this.HandleEvent("OnUpdate");
	this.Evaluate();
}

TriggerManager.prototype.OnGlobalAttacked = function(msg)
{
	this.HandleEvent("OnGlobalAttacked", msg);
}

Engine.RegisterComponentType(IID_TriggerManager, "TriggerManager", TriggerManager);
