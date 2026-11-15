#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Small local Marionette client for measurements in the official Firefox binary.

The port is bound by Firefox on target loopback. Browser commands are explicit;
no mock page state is used. Diagnostic profiles opt into system-context access.
"""
import json
import socket


class Marionette:
    def __init__(self, port=28290, timeout=10):
        self.socket = socket.create_connection(('127.0.0.1', port), timeout=timeout)
        self.sequence = 0
        self.hello = self.receive()

    def receive(self):
        header = bytearray()
        while True:
            byte = self.socket.recv(1)
            if not byte:
                raise EOFError('Marionette closed the connection')
            if byte == b':':
                break
            header.extend(byte)
            if len(header) > 12:
                raise ValueError('Invalid Marionette frame length')
        length = int(header)
        if length > 64 * 1024 * 1024:
            raise ValueError('Oversized Marionette reply')
        payload = bytearray()
        while len(payload) < length:
            byte = self.socket.recv(length - len(payload))
            if not byte:
                raise EOFError('Truncated Marionette reply')
            payload.extend(byte)
        return json.loads(payload)

    def command(self, name, parameters=None):
        self.sequence += 1
        payload = json.dumps([0, self.sequence, name, parameters or {}]).encode()
        self.socket.sendall(str(len(payload)).encode() + b':' + payload)
        response = self.receive()
        if response[:2] != [1, self.sequence]:
            raise RuntimeError('Unexpected Marionette response: ' + repr(response))
        if response[2]:
            raise RuntimeError(response[2])
        return response[3]

    def start(self):
        return self.command('WebDriver:NewSession', {'capabilities': {'alwaysMatch': {}}})

    def support(self):
        self.command('Marionette:SetContext', {'value': 'chrome'})
        return self.command('WebDriver:ExecuteAsyncScript', {
            'script': '''const done = arguments[arguments.length - 1];
            ChromeUtils.importESModule("resource://gre/modules/Troubleshoot.sys.mjs")
              .Troubleshoot.snapshot().then(done, e => done({error: String(e)}));''',
            'args': [], 'scriptTimeout': 15000, 'newSandbox': True,
        })

    def pages(self):
        self.command('Marionette:SetContext', {'value': 'content'})
        handles = self.command('WebDriver:GetWindowHandles')
        if isinstance(handles, dict):
            handles = handles['value']
        result = []
        for handle in handles:
            self.command('WebDriver:SwitchToWindow', {'handle': handle})
            result.append(self.command('WebDriver:ExecuteScript', {
                'script': "return {url:location.href,title:document.title,body:document.body.innerText.slice(0,4000),fixture:window.fxResult||null,video:window.fxVideoResult||null};",
                'args': [], 'newSandbox': False,
            }))
        return result

    def close(self):
        self.socket.close()
