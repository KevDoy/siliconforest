# SPDX-License-Identifier: MIT
"""Freeze the owned WebGL/CSS scene and retain its actual renderer coordinates."""
import json
import time


def measure(client, output):
    client.command('Marionette:SetContext', {'value': 'content'})
    for handle in client.command('WebDriver:GetWindowHandles'):
        client.command('WebDriver:SwitchToWindow', {'handle': handle})
        found = client.command('WebDriver:ExecuteScript', dict(script='return typeof fxResult === "object";',
                                                             args=[], newSandbox=False))
        if found:
            break
    else:
        raise RuntimeError('Owned reference scene absent')
    result = client.command('WebDriver:ExecuteAsyncScript', dict(script='''
      const done=arguments[arguments.length-1];scrollTo(0,0);
      for(const animation of document.getAnimations()) {animation.pause();animation.currentTime=0;}
      requestAnimationFrame(()=>requestAnimationFrame(()=>{
        const boxes={};for(const name of ['gl','motion']){
          const e=document.querySelector(name==='gl'?'#gl':'.motion'),r=e.getBoundingClientRect();
          const inset=name==='gl'?3:2;
          boxes[name]={x:(mozInnerScreenX+r.x+inset)*devicePixelRatio,
            y:(mozInnerScreenY+r.y+inset)*devicePixelRatio,
            width:(r.width-2*inset)*devicePixelRatio,height:(r.height-2*inset)*devicePixelRatio};
        }done({boxes,scale:devicePixelRatio,hash:fxResult.hash,glError:fxResult.glError,
          renderer:fxResult.renderer,visibility:document.visibilityState});
      }));''', args=[], newSandbox=False))
    for box in result['boxes'].values():
        if not (box['x'] >= 0 and box['y'] >= 0 and box['width'] > 0 and box['height'] > 0
                and box['x']+box['width'] <= 2560 and box['y']+box['height'] <= 1600):
            raise RuntimeError('Reference region outside admitted panel')
    time.sleep(.3)
    from renderer_capture import capture
    result['capture'] = capture(output.with_suffix('.ppm'))
    result['complete'] = (result['hash'] == 'e4bc0ce1' and result['glError'] == 0
                          and result['visibility'] == 'visible')
    output.write_text(json.dumps(result, indent=2)+'\n')
    return result
