#!/usr/bin/env python3
"""Derive R5 F00 from the hash-bound original-ROM/MAME capture, never a tone.

Default output is diagnostic candidates. --install writes only cd/AUDIO/F00.PCM
and this directory's manifest.json. Rebuilding the game uses the delivered PCM;
reauthoring it requires the separately retained original audio oracle capture.
"""
import argparse
import hashlib
import json
from pathlib import Path
import wave
import numpy as np
from scipy import signal

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
EVIDENCE=ROOT/'verification/r10_r5/machinegun'
SOURCE_WAV='44ec677a31e576f718f3ba2f98e3e2f98e1f02c7f70dbb9ca95f38f56131ea90'
SOURCE_EVENTS='a9247595c929a66bb27b45c893c291e3cc92a9022b3e971338424c2065517a0c'
OLD_PCM='26a466ac3a2426862603781dc1fe8e8f462706a34ab7a7f446c6533b5e3c8a7f'
def sha(data):return hashlib.sha256(data).hexdigest()
def rms(x):return float(np.sqrt(np.mean(np.asarray(x,dtype=float)**2)))
def snr(x,y):return float(20*np.log10(rms(x)/rms(np.asarray(x)-np.asarray(y))))
def padded(data):return data+bytes(-len(data)%32)
def spectrum(x):
    f,p=signal.welch(np.asarray(x,dtype=float),44100,nperseg=min(4096,len(x)))
    edges=(0,250,1000,4000,8000,11025,22051)
    return {'centroid_hz':float(np.dot(f,p)/sum(p)),
        'band_power_fraction':{f'{a}-{b}':float(sum(p[(f>=a)&(f<b)])/sum(p))
                              for a,b in zip(edges,edges[1:])}}
def scsp_half_rate(x):
    x=np.asarray(x,dtype=float);out=np.empty(len(x)*2)
    out[::2]=x;out[1:-1:2]=(x[:-1]+x[1:])/2;out[-1]=x[-1]/2
    return out

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture',type=Path,default=EVIDENCE/'original_audio_4_6_retry')
    parser.add_argument('--out',type=Path,default=EVIDENCE/'candidates')
    parser.add_argument('--install',action='store_true')
    args=parser.parse_args();capture=args.capture;out=args.out;out.mkdir(parents=True,exist_ok=True)
    raw=(capture/'original.wav').read_bytes();events_raw=(capture/'ym_events.jsonl').read_bytes()
    assert sha(raw)==SOURCE_WAV and sha(events_raw)==SOURCE_EVENTS,'Different original oracle capture'
    assert json.loads((capture/'capture_report.json').read_text())['passed'] is True
    with wave.open(str(capture/'original.wav')) as audio:
        assert (audio.getframerate(),audio.getsampwidth(),audio.getnchannels())==(44100,2,2)
        stereo=np.frombuffer(audio.readframes(audio.getnframes()),dtype='<i2').reshape(-1,2)
    assert np.array_equal(stereo[:,0],stereo[:,1]),'Original reference must be dual mono'
    recorded=stereo[:,0].astype(np.int32);events=[];register=None
    for line in events_raw.splitlines():
        e=json.loads(line)
        if e['port']==4:register=e['value']
        else:events.append(dict(e,register=register))
    def keyon(tick):
        return next(e['seconds'] for e in events if e['tick']==tick and e['register']==32 and e['value']&16)
    def keyoff(tick):
        return next(e['seconds'] for e in events if e['tick']==tick and e['register']==32 and not e['value']&16)
    # Include the sample containing the first channel key-on; retain every
    # captured sample until its natural silence. No filtering or normalization.
    start=int(keyon(20)*44100);window=recorded[start:start+22050]
    last=int(np.flatnonzero(window)[-1]);original=window[:last+17]
    pcm16=padded(original.astype('>i2').tobytes()+b'\0\0')
    (out/'F00_44100.PCM').write_bytes(pcm16)
    # Fixed x4 gain uses the available signed-8 range. Round to nearest,
    # ties to even, then append the same 16 zero tail samples and DMA guard.
    quantized=np.rint(original.astype(float)/64).astype(np.int32)
    assert quantized.min()>=-128 and quantized.max()<=127,'Quantization saturation'
    last8=int(np.flatnonzero(quantized)[-1]);samples=last8+17
    q=quantized[:samples].astype(np.int8)
    pcm8=padded(q.tobytes()+b'\0')
    assert samples==14926 and len(pcm8)==14944
    (out/'F00_44100_S8.PCM').write_bytes(pcm8)
    decoded=quantized.astype(float)*64
    active_end=round((keyoff(30)-keyon(20))*44100)
    active=original[:active_end].astype(float);decoded_active=decoded[:active_end]
    harpoon_start=round(keyon(240)*44100);harpoon_end=round(keyoff(264)*44100)
    harpoon_original=recorded[harpoon_start:harpoon_end].astype(float)
    harpoon=np.frombuffer((ROOT/'cd/AUDIO/F08.PCM').read_bytes()[:29179*2],dtype='>i2')
    harpoon_scsp=scsp_half_rate(harpoon)[:len(harpoon_original)]
    ratio=rms(active)/rms(harpoon_original)
    target_gain=rms(harpoon_scsp)/rms(harpoon_original)
    volume=103;mix_gain=4*10**(-(127-volume)*.75/20)
    baseline=out/'F00_R4_22050.PCM';current=(ROOT/'cd/AUDIO/F00.PCM').read_bytes()
    if sha(current)==OLD_PCM:baseline.write_bytes(current)
    old=None
    if baseline.exists():
        assert sha(baseline.read_bytes())==OLD_PCM
        old=scsp_half_rate(np.frombuffer(baseline.read_bytes()[:7461*2],dtype='>i2'))
    limited=signal.resample_poly(signal.resample_poly(original.astype(float),1,2),2,1)[:len(original)]
    repeats={}
    for name,ticks in [('four_tick',[160,164,168,172,176]),('six_tick',[80,86,92,98,104])]:
        anchor=int(keyon(ticks[0])*44100);end=int(keyon(ticks[-1])*44100)+len(original)
        actual=recorded[anchor:end].astype(float);predicted=np.zeros(len(actual))
        starts=[]
        for tick in ticks:
            offset=int(keyon(tick)*44100)-anchor;starts.append(offset)
            predicted[offset:]=0;n=min(len(decoded),len(actual)-offset)
            predicted[offset:offset+n]=decoded[:n]
        n=starts[-1]+active_end
        repeats[name]={'request_ticks':ticks,'keyon_sample_offsets':starts,
            'actual_spectrum':spectrum(actual[:n]),'fixed_sample_spectrum':spectrum(predicted[:n]),
            'waveform_correlation':float(np.corrcoef(actual[:n],predicted[:n])[0,1]),
            'limitation':'A reused PCM restarts its phase; original YM envelope/phase carries across requests. No sample-exact retrigger claim.'}
    report={'release':'R10_R5','scope':'Original-ROM/MAME source and host asset/format analysis; no physical Saturn audio claim',
        'source_wav_sha256':SOURCE_WAV,'source_events_sha256':SOURCE_EVENTS,
        'rom_sha256':json.loads((capture/'capture_report.json').read_text())['rom_sha256_before'],
        'original_entries':['BANK0 AD88/ADB4: command 00 through 2BAF','FIXED 788E request','FIXED 7822 periodic'],
        'keyon_seconds':keyon(20),'keyoff_seconds':keyoff(30),'crop_start_sample':start,
        'crop_16bit_samples':len(original),'crop_16bit_bytes':len(pcm16),'crop_16bit_sha256':sha(pcm16),
        'asset':{'file':'F00.PCM','samples':samples,'rate':44100,'bits':8,'signed':True,
            'volume':volume,'encoding_gain':4,'scsp_resident':True,'bytes':len(pcm8),
            'sha256':sha(pcm8),'priority_samples_44100':8448,'priority':80,
            'guard_samples':1,'zero_tail_samples':16,'quantization':'nearest signed int8 after fixed x4 gain; no clipping, dither, EQ or resampling'},
        'quantization':{'min_code':int(q.min()),'max_code':int(q.max()),'clipped_samples':0,
            'active_snr_db':snr(active,decoded_active),'full_decay_snr_db':snr(original,decoded),
            'active_correlation':float(np.corrcoef(active,decoded_active)[0,1]),
            'removed_tail':'Only trailing samples already quantized to zero; the reference 16-bit file retains the whole decay.'},
        'relative_level':{'original_gun_harpoon_rms_ratio':ratio,'current_harpoon_scsp_linear_rms':rms(harpoon_scsp),
            'target_gain_from_mame':target_gain,'selected_gain_from_mame':mix_gain,
            'selected_volume':volume,'nearest_step_error_db':float(20*np.log10(mix_gain/target_gain)),
            'predicted_gun_peak':float(np.max(np.abs(q.astype(float)))*256*mix_gain/4),
            'old_gun_peak':4280,'model':'Existing Jo TL mapping, nominal 0.75 dB per volume step; F08 uses SCSP half-rate linear interpolation.'},
        'spectral_controls':{'original_mame_44100':spectrum(active),'candidate_s8_44100':spectrum(decoded_active),
            'mame_bandlimited_to_22050':spectrum(limited[:active_end]),
            'old_C13_22050':spectrum(old[:active_end]) if old is not None else None,
            'C13_44100_reference':'Not present in the restored sources/disc payloads; no claim that all core/filter differences are due to sample rate.'},
        'repeat_cases':repeats,
        'memory':{'scsp_pcm_bytes':357440,'wram_pcm_bytes':493152,'music_ring_bytes':65568,
            'upload_room_bytes':97184,'F19_upload_bytes':93728,'F19_margin_bytes':3456,
            'delta_scsp_bytes':0,'delta_wram_bytes':0},
        'builder_sha256':sha(Path(__file__).read_bytes())}
    (out/'analysis.json').write_text(json.dumps(report,indent=2)+'\n')
    if args.install:
        (ROOT/'cd/AUDIO/F00.PCM').write_bytes(pcm8)
        (HERE/'manifest.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({'installed':args.install,'asset':report['asset'],'quantization':report['quantization'],
                      'relative_level':report['relative_level'],'memory':report['memory']},indent=2))

if __name__=='__main__':main()
