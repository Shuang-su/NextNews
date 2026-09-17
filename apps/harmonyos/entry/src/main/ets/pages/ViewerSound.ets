import { audio } from '@kit.AudioKit';
import { media } from '@kit.MediaKit';
import { BusinessError } from '@kit.BasicServicesKit';

// One player per current scene. Source identity and generation guard every async
// callback; a released/late player can never start audio for a different model.
export class ViewerSound {
  private player: media.AVPlayer | undefined = undefined;
  private source: string = '';
  private generation: number = 0;
  private unlocked: boolean = false;
  private foreground: boolean = true;
  private visible: boolean = false;
  private muted: boolean = false;
  private volume: number = 1;
  private changing: boolean = false;
  private focusBlocked: boolean = false;
  private duck: number = 1;
  private systemMuted: boolean = false;
  private report: (message: string) => void;
  constructor(report: (message: string) => void) { this.report = report; }
  configure(source: string): void {
    if (source === this.source) { return; }
    this.clear(); this.source = source;
    if (!source) { return; }
    if (!/^https:\/\//i.test(source)) { this.report('背景声音需要 HTTPS 地址'); return; }
    const request = this.generation;
    media.createAVPlayer().then((player: media.AVPlayer) => {
      if (request !== this.generation) { player.release().catch(() => {}); return; }
      this.player = player;
      player.on('stateChange', (state: string) => {
        if (request !== this.generation || player !== this.player) { return; }
        if (state === 'initialized') {
          player.prepare().catch((error: BusinessError) => { if (request === this.generation) { this.report(error.message); } });
        } else if (state === 'prepared') { player.loop = true; this.sync(); }
        else if (state === 'paused' || state === 'playing') { this.sync(); }
      });
      player.on('audioInterrupt', (event: audio.InterruptEvent) => {
        if (request !== this.generation || player !== this.player) { return; }
        if (event.hintType === audio.InterruptHint.INTERRUPT_HINT_PAUSE || event.hintType === audio.InterruptHint.INTERRUPT_HINT_STOP) { this.focusBlocked = true; }
        else if (event.hintType === audio.InterruptHint.INTERRUPT_HINT_RESUME) { this.focusBlocked = false; }
        else if (event.hintType === audio.InterruptHint.INTERRUPT_HINT_DUCK) { this.duck = .2; }
        else if (event.hintType === audio.InterruptHint.INTERRUPT_HINT_UNDUCK) { this.duck = 1; }
        else if (event.hintType === audio.InterruptHint.INTERRUPT_HINT_MUTE) { this.systemMuted = true; }
        else if (event.hintType === audio.InterruptHint.INTERRUPT_HINT_UNMUTE) { this.systemMuted = false; }
        this.sync();
      });
      player.on('error', (error: BusinessError) => { if (request === this.generation) { this.report(`背景声音：${error.message}`); } });
      player.url = source;
    }).catch((error: BusinessError) => { if (request === this.generation) { this.report(error.message); } });
  }
  gesture(): void { this.unlocked = true; this.sync(); }
  active(value: boolean): void { this.foreground = value; this.sync(); }
  shown(value: boolean): void { this.visible = value; this.sync(); }
  level(volume: number, muted: boolean): void {
    if (!Number.isFinite(volume)) { return; }
    this.volume = Math.max(0, Math.min(1, volume)); this.muted = muted; this.sync();
  }
  private sync(): void {
    const player = this.player;
    if (!player || !['prepared', 'playing', 'paused'].includes(player.state)) { return; }
    try { player.setVolume(this.muted || this.systemMuted ? 0 : this.volume * this.duck); }
    catch (error) { this.report(`背景声音：${(error as BusinessError).message}`); return; }
    if (this.changing) { return; }
    const play = this.unlocked && this.foreground && this.visible && !this.focusBlocked;
    if ((play && player.state === 'playing') || (!play && player.state !== 'playing')) { return; }
    const request = this.generation; this.changing = true;
    (play ? player.play() : player.pause()).catch((error: BusinessError) => {
      if (request === this.generation) { this.report(error.message); }
    }).finally(() => {
      if (request !== this.generation) { return; }
      this.changing = false;
      // A foreground/mute/scene change may have arrived during play/pause.
      if (play !== (this.unlocked && this.foreground && this.visible && !this.focusBlocked)) { this.sync(); }
    });
  }
  clear(): void {
    this.generation++; this.source = ''; this.changing = false; this.visible = false; this.focusBlocked = false; this.duck = 1; this.systemMuted = false;
    const old = this.player; this.player = undefined;
    if (old) { old.off('stateChange'); old.off('error'); old.off('audioInterrupt'); old.release().catch(() => {}); }
    this.report('');
  }
}
