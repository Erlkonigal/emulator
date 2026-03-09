#!/usr/bin/env python3
"""Simple menuconfig for hardware configuration."""

import os
import sys
import subprocess

def read_config(config_path):
    """Read current config values."""
    config = {}
    if os.path.exists(config_path):
        with open(config_path, 'r') as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith('#'):
                    continue
                if '=' in line:
                    key, value = line.split('=', 1)
                    config[key.strip()] = value.strip()
    return config

def write_config(config_path, config):
    """Write config values back to file."""
    lines = [
        '# Hardware Configuration',
        '# Format: KEY=VALUE (hex values with 0x prefix)',
        '',
    ]
    
    order = [
        'ROM_BASE', 'RAM_BASE', 'RAM_SIZE',
        'UART_BASE', 'UART_SIZE',
        'DEFAULT_DEBUG_PORT',
        'CPU_FREQUENCY',
        'COMMIT_QUEUE_SIZE',
        'MAX_NUM_REGISTERS', 'MAX_NUM_CSR',
        'MAX_NUM_COMMITS_PER_CYCLE', 'MAX_NUM_COMMITS_PER_CONSUMPTION',
        'PADDING',
        'MAX_INST_DECODE_LEN',
    ]
    
    for key in order:
        if key in config:
            lines.append(f'{key}={config[key]}')
    
    lines.append('')
    
    with open(config_path, 'w') as f:
        f.write('\n'.join(lines))

def show_menu(config):
    """Display menu and handle user input."""
    while True:
        print('\n' + '='*60)
        print('Hardware Configuration Menu')
        print('='*60)
        print(f'  1. ROM Base Address      : {config.get("ROM_BASE", "0x00000000")}')
        print(f'  2. RAM Base Address      : {config.get("RAM_BASE", "0x80000000")}')
        print(f'  3. RAM Size              : {config.get("RAM_SIZE", "0x10000000")}')
        print(f'  4. UART Base Address     : {config.get("UART_BASE", "0x10000000")}')
        print(f'  5. UART Size             : {config.get("UART_SIZE", "0x1000")}')
        print(f'  6. Default Debug Port    : {config.get("DEFAULT_DEBUG_PORT", "1234")}')
        print(f'  7. CPU Frequency (Hz)    : {config.get("CPU_FREQUENCY", "1000000")}')
        print(f'  8. Commit Queue Size     : {config.get("COMMIT_QUEUE_SIZE", "1024")}')
        print(f'  9. Max Num Registers     : {config.get("MAX_NUM_REGISTERS", "32")}')
        print(f' 10. Max Num CSR           : {config.get("MAX_NUM_CSR", "64")}')
        print(f' 11. Max Commits/Cycle     : {config.get("MAX_NUM_COMMITS_PER_CYCLE", "8")}')
        print(f' 12. Max Commits/Consump.  : {config.get("MAX_NUM_COMMITS_PER_CONSUMPTION", "256")}')
        print(f' 13. Padding               : {config.get("PADDING", "64")}')
        print(f' 14. Max Inst Decode Len   : {config.get("MAX_INST_DECODE_LEN", "64")}')
        print('-'*60)
        print('  s. Save and Exit')
        print('  q. Quit without saving')
        print('  r. Reset to defaults')
        print('='*60)
        
        choice = input('Select option: ').strip().lower()
        
        if choice == 's':
            return True
        elif choice == 'q':
            return False
        elif choice == 'r':
            config = {
                'ROM_BASE': '0x00000000',
                'RAM_BASE': '0x80000000',
                'RAM_SIZE': '0x10000000',
                'UART_BASE': '0x10000000',
                'UART_SIZE': '0x1000',
                'DEFAULT_DEBUG_PORT': '1234',
                'CPU_FREQUENCY': '1000000',
                'COMMIT_QUEUE_SIZE': '1024',
                'MAX_NUM_REGISTERS': '32',
                'MAX_NUM_CSR': '64',
                'MAX_NUM_COMMITS_PER_CYCLE': '8',
                'MAX_NUM_COMMITS_PER_CONSUMPTION': '256',
                'PADDING': '64',
                'MAX_INST_DECODE_LEN': '64',
            }
            print('Reset to default values.')
        else:
            try:
                idx = int(choice)
                keys = [
                    'ROM_BASE', 'RAM_BASE', 'RAM_SIZE',
                    'UART_BASE', 'UART_SIZE', 'DEFAULT_DEBUG_PORT',
                    'CPU_FREQUENCY', 'COMMIT_QUEUE_SIZE',
                    'MAX_NUM_REGISTERS', 'MAX_NUM_CSR',
                    'MAX_NUM_COMMITS_PER_CYCLE', 'MAX_NUM_COMMITS_PER_CONSUMPTION',
                    'PADDING', 'MAX_INST_DECODE_LEN',
                ]
                if 1 <= idx <= len(keys):
                    key = keys[idx - 1]
                    current = config.get(key, '')
                    new_val = input(f'Enter new value for {key} [{current}]: ').strip()
                    if new_val:
                        config[key] = new_val
            except ValueError:
                pass

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_dir = os.path.dirname(script_dir)
    config_path = os.path.join(project_dir, 'config', 'hardware.conf')
    gen_script = os.path.join(script_dir, 'gen_config.py')
    
    config = read_config(config_path)
    
    if show_menu(config):
        write_config(config_path, config)
        print(f'\nConfiguration saved to: {config_path}')
        
        subprocess.run([sys.executable, gen_script], check=True)
        print('Configuration header generated.')
    else:
        print('\nConfiguration not saved.')

if __name__ == '__main__':
    main()