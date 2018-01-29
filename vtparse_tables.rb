
class StateTransition
    attr_accessor :to_state
    def initialize(to_state)
        @to_state = to_state
    end
end

def transition_to(state)
    StateTransition.new(state)
end

$states = {}

$anywhere_transitions = {
    0x18       => [:execute, transition_to(:GROUND)],
    0x1a       => [:execute, transition_to(:GROUND)],
    #0x80..0x8f => [:execute, transition_to(:GROUND)],
    #0x91..0x97 => [:execute, transition_to(:GROUND)],
    #0x99       => [:execute, transition_to(:GROUND)],
    #0x9a       => [:execute, transition_to(:GROUND)],
    #0x9c       => transition_to(:GROUND),
    0x1b       => transition_to(:ESCAPE),
    #0x98       => transition_to(:SOS_PM_APC_STRING),
    #0x9e       => transition_to(:SOS_PM_APC_STRING),
    #0x9f       => transition_to(:SOS_PM_APC_STRING),
    #0x90       => transition_to(:DCS_ENTRY),
    #0x9d       => transition_to(:OSC_STRING),
    #0x9b       => transition_to(:CSI_ENTRY),
}

$states[:GROUND] = {
    0x00..0x17 => :execute,
    0x19       => :execute,
    0x1c..0x1f => :execute,
    0x20..0x7f => :print,
    0xc2..0xdf => [:print, transition_to(:UTF8_2)],
    0xe0       => [:print, transition_to(:UTF8_3_E0)],
    0xe1..0xec => [:print, transition_to(:UTF8_3)],
    0xed       => [:print, transition_to(:UTF8_3_ED)],
    0xee..0xef => [:print, transition_to(:UTF8_3)],
    0xf0       => [:print, transition_to(:UTF8_4_F0)],
    0xf1..0xf3 => [:print, transition_to(:UTF8_4)],
    0xf4       => [:print, transition_to(:UTF8_4_F4)],
}

$states[:UTF8_2] = {
    :on_entry  => :clear,
    0x80..0xbf => [:print, transition_to(:GROUND)],
}

$states[:UTF8_3_E0] = {
    :on_entry  => :clear,
    0xa0..0xbf => [:print, transition_to(:UTF8_3_1)],
}

$states[:UTF8_3_ED] = {
    :on_entry  => :clear,
    0x80..0x9f => [:print, transition_to(:UTF8_3_1)],
}

$states[:UTF8_3] = {
    :on_entry  => :clear,
    0x80..0xbf => [:print, transition_to(:UTF8_3_1)],
}

$states[:UTF8_3_1] = {
    0x80..0xbf => [:print, transition_to(:GROUND)],
}

$states[:UTF8_4_F0] = {
    :on_entry  => :clear,
    0x90..0xbf => [:print, transition_to(:UTF8_4_1)],
}

$states[:UTF8_4_F4] = {
    :on_entry  => :clear,
    0x80..0x8f => [:print, transition_to(:UTF8_4_1)],
}

$states[:UTF8_4] = {
    :on_entry  => :clear,
    0x80..0xbf => [:print, transition_to(:UTF8_4_1)],
}

$states[:UTF8_4_1] = {
    0x80..0xbf => [:print, transition_to(:UTF8_4_2)],
}

$states[:UTF8_4_2] = {
    0x80..0xbf => [:print, transition_to(:GROUND)],
}

$states[:ESCAPE] = {
    :on_entry  => :clear,
    0x00..0x17 => :execute,
    0x19       => :execute,
    0x1c..0x1f => :execute,
    0x7f       => :ignore,
    0x20..0x2f => [:collect, transition_to(:ESCAPE_INTERMEDIATE)],
    0x30..0x4f => [:esc_dispatch, transition_to(:GROUND)],
    0x51..0x57 => [:esc_dispatch, transition_to(:GROUND)],
    0x59       => [:esc_dispatch, transition_to(:GROUND)],
    0x5a       => [:esc_dispatch, transition_to(:GROUND)],
    0x5c       => [:esc_dispatch, transition_to(:GROUND)],
    0x60..0x7e => [:esc_dispatch, transition_to(:GROUND)],
    0x5b       => transition_to(:CSI_ENTRY),
    0x5d       => transition_to(:OSC_STRING),
    0x50       => transition_to(:DCS_ENTRY),
    0x58       => transition_to(:SOS_PM_APC_STRING),
    0x5e       => transition_to(:SOS_PM_APC_STRING),
    0x5f       => transition_to(:SOS_PM_APC_STRING),
}

$states[:ESCAPE_INTERMEDIATE] = {
    0x00..0x17 => :execute,
    0x19       => :execute,
    0x1c..0x1f => :execute,
    0x20..0x2f => :collect,
    0x7f       => :ignore,
    0x30..0x7e => [:esc_dispatch, transition_to(:GROUND)]
}

$states[:CSI_ENTRY] = {
    :on_entry  => :clear,
    0x00..0x17 => :execute,
    0x19       => :execute,
    0x1c..0x1f => :execute,
    0x7f       => :ignore,
    0x20..0x2f => [:collect, transition_to(:CSI_INTERMEDIATE)],
    0x3a       => transition_to(:CSI_IGNORE),
    0x30..0x39 => [:param, transition_to(:CSI_PARAM)],
    0x3b       => [:param, transition_to(:CSI_PARAM)],
    0x3c..0x3f => [:collect, transition_to(:CSI_PARAM)],
    0x40..0x7e => [:csi_dispatch, transition_to(:GROUND)]
}

$states[:CSI_IGNORE] = {
    0x00..0x17 => :execute,
    0x19       => :execute,
    0x1c..0x1f => :execute,
    0x20..0x3f => :ignore,
    0x7f       => :ignore,
    0x40..0x7e => transition_to(:GROUND),
}

$states[:CSI_PARAM] = {
    0x00..0x17 => :execute,
    0x19       => :execute,
    0x1c..0x1f => :execute,
    0x30..0x39 => :param,
    0x3b       => :param,
    0x7f       => :ignore,
    0x3a       => transition_to(:CSI_IGNORE),
    0x3c..0x3f => transition_to(:CSI_IGNORE),
    0x20..0x2f => [:collect, transition_to(:CSI_INTERMEDIATE)],
    0x40..0x7e => [:csi_dispatch, transition_to(:GROUND)]
}

$states[:CSI_INTERMEDIATE] = {
    0x00..0x17 => :execute,
    0x19       => :execute,
    0x1c..0x1f => :execute,
    0x20..0x2f => :collect,
    0x7f       => :ignore,
    0x30..0x3f => transition_to(:CSI_IGNORE),
    0x40..0x7e => [:csi_dispatch, transition_to(:GROUND)],
}

$states[:DCS_ENTRY] = {
    :on_entry  => :clear,
    0x00..0x17 => :ignore,
    0x19       => :ignore,
    0x1c..0x1f => :ignore,
    0x7f       => :ignore,
    0x3a       => transition_to(:DCS_IGNORE),
    0x20..0x2f => [:collect, transition_to(:DCS_INTERMEDIATE)],
    0x30..0x39 => [:param, transition_to(:DCS_PARAM)],
    0x3b       => [:param, transition_to(:DCS_PARAM)],
    0x3c..0x3f => [:collect, transition_to(:DCS_PARAM)],
    0x40..0x7e => [transition_to(:DCS_PASSTHROUGH)]
}

$states[:DCS_INTERMEDIATE] = {
    0x00..0x17 => :ignore,
    0x19       => :ignore,
    0x1c..0x1f => :ignore,
    0x20..0x2f => :collect,
    0x7f       => :ignore,
    0x30..0x3f => transition_to(:DCS_IGNORE),
    0x40..0x7e => transition_to(:DCS_PASSTHROUGH)
}

$states[:DCS_IGNORE] = {
    0x00..0x17 => :ignore,
    0x19       => :ignore,
    0x1c..0x1f => :ignore,
    0x20..0x7f => :ignore,
}

$states[:DCS_PARAM] = {
    0x00..0x17 => :ignore,
    0x19       => :ignore,
    0x1c..0x1f => :ignore,
    0x30..0x39 => :param,
    0x3b       => :param,
    0x7f       => :ignore,
    0x3a       => transition_to(:DCS_IGNORE),
    0x3c..0x3f => transition_to(:DCS_IGNORE),
    0x20..0x2f => [:collect, transition_to(:DCS_INTERMEDIATE)],
    0x40..0x7e => transition_to(:DCS_PASSTHROUGH)
}

$states[:DCS_PASSTHROUGH] = {
    :on_entry  => :hook,
    0x00..0x17 => :put,
    0x19       => :put,
    0x1c..0x1f => :put,
    0x20..0x7e => :put,
    0x7f       => :ignore,
    :on_exit   => :unhook
}

$states[:SOS_PM_APC_STRING] = {
    0x00..0x17 => :ignore,
    0x19       => :ignore,
    0x1c..0x1f => :ignore,
    0x20..0x7f => :ignore,
}

$states[:OSC_STRING] = {
    :on_entry  => :osc_start,
    0x00..0x17 => :ignore,
    0x19       => :ignore,
    0x1c..0x1f => :ignore,
    0x20..0x7f => :osc_put,
    :on_exit   => :osc_end
}


# get the list of actions implicit in the tables

action_names = {}
$states.each { |state, transitions|
    transitions.each { |keys, actions|
        actions = [actions] if !actions.kind_of?(Array)
        actions.each { |action|
            if action.kind_of?(Symbol)
                action_names[action] = 1
            end
        }
    }
}

# establish an ordering to the states and actions

$actions_in_order = action_names.keys.sort   { |a1, a2| a1.to_s <=> a2.to_s } + [:error]
$states_in_order  = $states.keys.sort  { |s1, s2| s1.to_s <=> s2.to_s }



#
# Expand the above range-based data structures (which are convenient
# to write) into fully expanded tables (which are easier to use).
#

$state_tables = {}

def expand_ranges(hash_with_ranges_as_keys)
    array = []
    hash_with_ranges_as_keys.each { |range, val|
        if range.kind_of?(Range)
            range.each { |i|
                array[i] = val
            }
        elsif range.kind_of?(Fixnum)
            array[range] = val
        end
    }

    array
end

$states.each { |state, transitions|
    $state_tables[state] = expand_ranges(transitions)
}

# seed all the states with the anywhere transitions
$anywhere_transitions = expand_ranges($anywhere_transitions)

$state_tables.each { |state, transitions|
    $anywhere_transitions.each_with_index { |transition, i|
        next if transition.nil?
        if transitions[i]
            raise "State #{state} already had a transition defined for 0x#{i.to_s(16)}, but " + \
                  "that transition is also an anywhere transition!"
        end
        transitions[i] = transition
    }
}

# for consistency, make all transitions *lists* of actions
$state_tables.each { |state, transitions|
    transitions.map! { |t| t.kind_of?(Array) ? t : [t] }
}

