# SPDX-License-Identifier: MIT
"""Request quality only on the admitted own public video; verify real dimensions.

An available/requested quality label alone is never a1080p or60fps result.
No account/advertisement/consent interaction or pointer/key input is performed.
"""
import json
import time


def admissible(state,video_id):
    return (state.get('videoId')==video_id and state.get('ad') is False and
            state.get('width',0)>=1920 and state.get('height',0)>=1080 and
            state.get('readyState',0)>=2 and state.get('error') is None)


def prepare(client,video_id,output,seconds=20):
    result=dict(requested_quality='hd1080',input_injected=False,complete=False,states=[])
    def script(source):
        return client.command('WebDriver:ExecuteScript',dict(script=source,args=[video_id],newSandbox=False))
    try:
        request=script('''const p=document.querySelector('#movie_player'),v=document.querySelector('video');
          const d=p?.getVideoData?.();
          if(d?.video_id!==arguments[0]||p?.classList.contains('ad-showing')||!v)
            throw new Error('Own public non-ad video identity changed');
          const available=p.getAvailableQualityLevels?.()||[];
          const range=typeof p.setPlaybackQualityRange==='function',single=typeof p.setPlaybackQuality==='function';
          if(range)p.setPlaybackQualityRange('hd1080','hd1080');
          if(single)p.setPlaybackQuality('hd1080');
          v.muted=true;v.play().catch(()=>{});
          return {videoId:d.video_id,available,range,single};''')
        result['request']=request
        deadline=time.monotonic()+seconds
        while time.monotonic()<deadline:
            state=script('''const p=document.querySelector('#movie_player'),v=document.querySelector('video'),d=p?.getVideoData?.();
              let stats=null;try{const s=p?.getVideoStats?.();if(s)stats={fmt:s.fmt,afmt:s.afmt};}catch(e){}
              return {videoId:d?.video_id,ad:p?.classList.contains('ad-showing'),
                width:v?.videoWidth,height:v?.videoHeight,readyState:v?.readyState,error:v?.error?.code||null,
                quality:p?.getPlaybackQuality?.(),stats};''')
            result['states'].append(state)
            if state.get('videoId')!=video_id or state.get('ad') is not False:
                raise RuntimeError('Video identity or non-ad admission changed; no interaction attempted')
            if admissible(state,video_id):
                result['complete']=True
                return result
            time.sleep(.25)
        raise RuntimeError('Requested1080 quality did not deliver actual1920x1080 video before deadline')
    except BaseException as error:
        result['error']=repr(error)
        raise
    finally:output.write_text(json.dumps(result,indent=2)+'\n')
