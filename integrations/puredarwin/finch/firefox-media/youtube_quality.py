# SPDX-License-Identifier: MIT
"""Request quality only on the admitted own public video; verify real dimensions.

An available/requested quality label alone is never a1080p or60fps result.
No account/advertisement/consent interaction or pointer/key input is performed.
"""
import json
import time

STATE_SCRIPT='''const p=document.querySelector('#movie_player'),v=document.querySelector('video'),d=p?.getVideoData?.();
  let stats=null,selectedFormat=null;
  try{const s=p?.getVideoStats?.();if(s)stats={fmt:s.fmt,afmt:s.afmt};
    const response=p?.getPlayerResponse?.(),formats=response?.streamingData?.adaptiveFormats||[];
    const f=formats.find(f=>String(f.itag)===String(s?.fmt));
    if(f)selectedFormat={itag:f.itag,mimeType:f.mimeType,width:f.width,height:f.height,
      fps:f.fps,bitrate:f.bitrate,colorInfo:f.colorInfo};
  }catch(e){}
  return {videoId:d?.video_id,ad:p?.classList.contains('ad-showing'),
    width:v?.videoWidth,height:v?.videoHeight,readyState:v?.readyState,error:v?.error?.code||null,
    quality:p?.getPlaybackQuality?.(),stats,selectedFormat};'''


def describe(client):
    return client.command('WebDriver:ExecuteScript',dict(script=STATE_SCRIPT,args=[],newSandbox=False))


def capabilities(client,state):
    fmt=state.get('selectedFormat')
    if not fmt:return dict(observed=False,reason='No identity-matched selected itag/format; available formats do not identify the decoder')
    result=client.command('WebDriver:ExecuteAsyncScript',dict(script='''
      const f=arguments[0],done=arguments[arguments.length-1];
      Promise.resolve().then(()=>navigator.mediaCapabilities.decodingInfo({type:'media-source',video:{contentType:f.mimeType,
        width:f.width,height:f.height,bitrate:f.bitrate,framerate:f.fps}})).then(
        r=>done({observed:true,supported:r.supported,smooth:r.smooth,powerEfficient:r.powerEfficient}),
        e=>done({observed:false,error:String(e)}));''',args=[fmt],newSandbox=False,scriptTimeout=10000))
    return result


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
            state=script(STATE_SCRIPT)
            result['states'].append(state)
            if state.get('videoId')!=video_id or state.get('ad') is not False:
                raise RuntimeError('Video identity or non-ad admission changed; no interaction attempted')
            if admissible(state,video_id):
                result['capabilities']=capabilities(client,state)
                result['complete']=True
                return result
            time.sleep(.25)
        raise RuntimeError('Requested1080 quality did not deliver actual1920x1080 video before deadline')
    except BaseException as error:
        result['error']=repr(error)
        raise
    finally:output.write_text(json.dumps(result,indent=2)+'\n')
