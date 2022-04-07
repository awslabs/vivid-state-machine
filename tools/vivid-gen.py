# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: Apache-2.0.

import json
import os
import sys
from pathlib import Path
if __name__ == '__main__':
    import time
    from watchdog.observers import Observer
    from watchdog.events import FileSystemEventHandler
    import argparse


class VividSmParser():
    INDENT_WIDTH = 2

    def __init__(self, filename, output_directory):
        if not any(filename.lower().endswith(ext) for ext in ['.c', '.cpp', '.cc', '.cxx']):
            return
        self.parse_file(filename)
        if not self.states:
            return
        self.write_puml(filename, output_directory)

    def get_arg(self):
        start_index = self.index
        paren_level = 0
        bracket_level = 0
        in_string = False
        in_char = False
        in_escape = False
        in_comment = False
        in_cpp_comment = False
        last_char = ''
        out = ""
        while self.index < len(self.code):
            char = self.code[self.index]
            comment_ended = False
            if char == '\\':
                if in_string or in_char:
                    in_escape = not in_escape
            elif char == '\"':
                if not in_comment and not in_char and (not in_string or not in_escape):
                    in_string = not in_string
            elif char == '\'':
                if not in_comment and not in_string and (not in_char or not in_escape):
                    in_char = not in_char
            elif char == '*':
                if not in_comment and not in_char and not in_string and last_char == '/':
                    in_comment = True
                    in_cpp_comment = False
            elif char == '/':
                if in_comment and not in_cpp_comment and last_char == '*':
                    comment_ended = True
                elif not in_comment and not in_char and not in_string and last_char == '/':
                    in_comment = True
                    in_cpp_comment = True
            elif char == '\n':
                if in_comment and in_cpp_comment:
                    comment_ended = True
            elif char == ',':
                if not in_comment and not in_string and not in_char:
                    if paren_level == 0 and bracket_level == 0:
                        self.index += 1 # swallow the comma
                        return out.strip()
            elif char == '{':
                if not in_comment and not in_string and not in_char:
                    bracket_level += 1
            elif char == '}':
                if not in_comment and not in_string and not in_char:
                    bracket_level -= 1
            elif char == '(':
                if not in_comment and not in_string and not in_char:
                    paren_level += 1
            elif char == ')':
                if not in_comment and not in_string and not in_char:
                    if paren_level == 0 and bracket_level == 0:
                        return out.strip()
                    paren_level -= 1

            if char != '\\' and in_escape:
                in_escape = False
            if last_char == '/' and not in_comment:
                out += '/'
            if not in_string and char.isspace():
                char = ' '
            if char != '/' and not in_comment and (char != ' ' or len(out) == 0  or out[-1] != ' '):
                out += char
            last_char = '' if comment_ended else char
            if comment_ended:
                in_comment = False
            self.index += 1
        return out.strip()

    def add_state(self, state):
        if state not in self.states:
            self.states[state] = {'children': [], 'transitions': [], 'parallel_children': False, 'json_props': {}, 'type': 'ROOT'}

    def get_json(self, json_string):
        try:
            return json.loads(json_string)
        except:
            return {}

    def parse_file(self, filename):
        self.index = 0
        sub_states = ['VIVID_SUB_STATE', 'VIVID_SUB_STATE_FINAL', 'VIVID_SUB_STATE_PARALLEL', 'VIVID_SUB_JUNCTION', 'VIVID_SUB_CONDITION']
        transitions = ["VIVID_DEFAULT", "VIVID_ON_EVENT", "VIVID_ON_TIMEOUT", 'VIVID_JUMP', 'VIVID_ON_EVENT_PARAM', 'VIVID_JUMP_PARAM']
        with open(filename, 'r') as fp:
            self.code = fp.read()
            self.index = 0
            self.states = {}
            self.sm = []
            current_state = ''
            try:
                while True:
                    # Find the start of a VIVID_ macro
                    self.index = self.code.find('VIVID_', self.index)
                    if self.index < 0:
                        break
                    macro_start = self.index
                    # Find the end of the macro
                    while self.code[self.index] in "ABCDEFGHIJKLMNOPQRSTUVWXYZ_":
                        self.index += 1
                    macro = self.code[macro_start:self.index]
                    # Skip over whitespace before the opening paren
                    while self.code[self.index].isspace():
                        self.index += 1
                    # Skip if not a function-like-macro
                    if self.code[self.index] != '(':
                        continue
                    self.index += 1
                    # Switch on the macro
                    if macro == "VIVID_STATE" or macro == "VIVID_STATE_CPP":
                        module = self.get_arg()
                        current_state = self.get_arg()
                        self.add_state(current_state)
                    elif macro in sub_states:
                        sub_state = self.get_arg()
                        self.add_state(sub_state)
                        self.states[current_state]['children'].append(sub_state)
                        self.states[sub_state]['type'] = macro[len("VIVID_SUB_"):]
                        self.states[sub_state]['json_props'] = self.get_json(self.get_arg())
                        self.states[sub_state]['parent'] = current_state
                        if macro == 'VIVID_SUB_STATE_PARALLEL':
                            self.states[sub_state]['parallel_children'] = True
                    elif macro == "VIVID_ON_ENTRY":
                        self.states[current_state]['entry_action'] = self.get_arg()
                    elif macro == "VIVID_ON_EXIT":
                        self.states[current_state]['exit_action'] = self.get_arg()
                    elif macro in transitions:
                        transition = {'guard': 'true'}
                        if macro != 'VIVID_DEFAULT':
                            if macro != 'VIVID_JUMP':
                                transition['trigger'] = self.get_arg()
                            if macro == 'VIVID_ON_TIMEOUT':
                                transition['timeout'] = self.get_arg()
                            transition['guard'] = self.get_arg()
                        transition['target'] = self.get_arg()
                        transition['action'] = self.get_arg()
                        transition['json_props'] = self.get_json(self.get_arg())
                        transition['type'] = macro[len("VIVID_"):].replace('_PARAM', '')
                        self.states[current_state]['transitions'].append(transition)
                    elif macro == 'VIVID_CREATE_SM':
                        binding = self.get_arg()
                        name = self.get_arg()
                        if name.find('"') >= 0: # if c string
                            name = self.get_json(name)
                        else:
                            name = str(len(self.sm) + 1)
                        self.sm.append({'name': name, 'root_state': self.get_arg()})
            except IndexError:
                pass
        if not self.sm:
            self.sm.append({'root_state': 'root'})

    def is_action(self, action):
        return action != '' and action != 'VIVID_NO_ACTION'

    def add_code(self, code, add_newlines):
        in_escape = False
        in_string = False
        in_char = False
        index = 0
        out = ""
        while index < len(code):
            char = code[index]
            index += 1
            if char == '\\':
                out += '\\\\'
                in_escape = not in_escape
            else:
                out += char
                if char == ';':
                    # Add newlines after semicolons, except when in a string or character literal
                    if add_newlines and not in_string and not in_char:
                        if index < len(code):
                            out += '\\n'
                            if code[index] == ' ':
                                index += 1
                elif char == '"':
                    if not in_char and (not in_string or not in_escape):
                        in_string = not in_string
                elif char == "'":
                    if not in_string and (not in_char or not in_escape):
                        in_char = not in_char
                in_escape = False
        self.fp.write(out)

    def add_stereotype_puml(self, indent, state_name):
        state_type = self.states[state_name]['type']
        if state_type == 'CONDITION':
            stereotype = "choice"
        elif state_type == 'JUNCTION' or state_type == 'STATE_FINAL':
            stereotype = "end"
        else:
            return
        indent_string = ' ' * indent * self.INDENT_WIDTH
        self.fp.write(f'{indent_string}state {state_name} <<{stereotype}>>\n')

    def add_transitions_puml(self, indent, state_name, is_default):
        indent_string = ' ' * indent * self.INDENT_WIDTH
        state = self.states[state_name]
        for transition in state['transitions']:
            if (transition['type'] == 'DEFAULT') != is_default:
                continue
            target_state_name = transition['target']
            self.add_stereotype_puml(indent, state_name)
            if target_state_name not in self.states: # including NULL
                self.fp.write(indent_string + state_name)
            else:
                self.add_stereotype_puml(indent, target_state_name)
                direction = '' if 'dir' not in state['json_props'] else state['json_props']['dir']
                source = '[*]' if transition['type'] == 'DEFAULT' else state_name
                self.fp.write(f'{indent_string}{source} -{direction}-> {target_state_name}')
            is_guard_true = transition['guard'] == 'true'
            is_condition = state['type'] == 'CONDITION'
            if transition['type'] in ['DEFAULT', 'JUMP']:
                if is_condition or not is_guard_true or self.is_action(transition['action']):
                    self.fp.write(' : ')
            elif transition['type'] == 'ON_EVENT':
                self.fp.write(f" : {transition['trigger']}")
            elif transition['type'] == 'ON_TIMEOUT':
                self.fp.write(f" : {transition['trigger']}(")
                self.add_code(transition['timeout'], False)
                self.fp.write(')')
            else:
                raise Exception('Unknown transition type')
            if is_condition or not is_guard_true:
                self.fp.write('[')
                if is_condition and is_guard_true:
                    self.fp.write('else')
                else:
                    self.add_code(transition['guard'], False)
                self.fp.write(']')
            if self.is_action(transition['action']):
                self.fp.write('/')
                if transition['type'] in ['ON_EVENT', 'ON_TIMEOUT'] or is_condition or not is_guard_true:
                    self.fp.write('\\n')
                self.add_code(transition['action'], True)
            self.fp.write('\n')
            if 'note' in transition['json_props']:
                pos = '' if 'pos' not in transition['json_props'] else transition['json_props']['pos']
                self.fp.write(f"note {pos} on link\n{transition['json_props']['note']}\nend note\n")

    def walk_puml(self, state_name, indent):
        indent_string = ' ' * indent * self.INDENT_WIDTH
        if state_name not in self.states:
            return
        state = self.states[state_name]
        if 'note' in state['json_props']:
            if 'pos' in state['json_props']:
                self.fp.write(f"note {state['json_props']['pos']} of {state_name}\n{state['json_props']['note']}\nend note\n")
            else:
                self.fp.write(f"note as note_{state_name}\n{state['json_props']['note']}\nend note\n")
        for direction in ['entry', 'exit']:
            action = f'{direction}_action'
            if action in state and self.is_action(state[action]):
                self.fp.write(f"{indent_string}{state_name} : {direction}/")
                self.add_code(state[action], False)
                self.fp.write('\n')
        self.add_transitions_puml(indent, state_name, False)
        if state['children']:
            is_root = 'parent' not in state
            parallel_siblings = not is_root and self.states[state['parent']]['parallel_children']
            if parallel_siblings:
                if self.states[state['parent']]['children']:
                    sep = '||' if not 'sep' in parent['json_props'] else parent['json_props']['sep']
                    self.fp.write(indent_string + sep + "\n")
            elif not is_root:
                self.fp.write(f"{indent_string}state {state_name} \n")
            child_indent = indent + (0 if (parallel_siblings or is_root) else 1)
            self.add_transitions_puml(indent, state_name, True)
            for child in state['children']:
                self.walk_puml(child, child_indent)
            if not parallel_siblings and not is_root:
                self.fp.write(indent_string + "}\n")
    
    def write_puml(self, input_filename, output_directory):
        for sm in self.sm:
            path = Path(input_filename)
            if len(self.sm) > 1:
                path = path.with_name(f"{path.stem}_{sm.name}.puml")
            else:
                path = path.with_suffix('.puml')
            output_filename = os.path.join(output_directory, path.name)
            print(f"writing to {output_filename}")
            with open(output_filename, 'w') as self.fp:
                self.fp.write('@startuml\n')
                self.walk_puml(sm['root_state'], 0)
                self.fp.write('@enduml\n')


if __name__ == '__main__':
    class Handler(FileSystemEventHandler):
        @staticmethod
        def on_any_event(event):
            global args
            if event.is_directory:
                return
            if event.event_type == 'created' or event.event_type == 'modified':
                try:
                    VividSmParser(event.src_path, args.output)
                except:
                    pass

    print('vivid-gen')

    parser = argparse.ArgumentParser(description='Generates PlantUML state diagram from a Vivid State Machine')
    parser.add_argument('-i', '--input', default='.', help='Input file or directory. Default: current working directory.')
    parser.add_argument('-m', '--monitor', action='store_true', help='File monitoring is performed.')
    parser.add_argument('-o', '--output', default='out', help='Output directory. Default: out')
    args = parser.parse_args()    
    
    if not os.path.exists(args.output):
        os.mkdir(args.output)
    output_map = {}
    if os.path.isfile(args.input):
        VividSmParser(args.input, args.output)
    else:
        for root, subdirs, files in os.walk(args.input):
            for filename in files:
                file_path = os.path.join(root, filename)
                VividSmParser(file_path, args.output)
    if args.monitor:
        print(f"monitoring '{args.input}' for changes")
        observer = Observer()
        event_handler = Handler()
        observer.schedule(event_handler, args.input, recursive=True)
        observer.start()
        try:
            while True:
                time.sleep(1)
        except KeyboardInterrupt:
            observer.stop()
        observer.join()
