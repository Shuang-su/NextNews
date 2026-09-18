import { webview } from '@kit.ArkWeb';
@Entry
@Component
struct WebComparison {
  private controller: webview.WebviewController = new webview.WebviewController();
  @State message: string = '同源 SuperSplat / ArkWeb / WebGL2 · 等待首帧';
  @State budget: number = 2;
  @State reference: string = 'SuperSplat';
  private runId: number = Date.now();
  private url(): string { return `http://127.0.0.1:${this.reference === 'MetaFlow' ? 8784 : 8768}/web-compare.html?content=lod-meta.json&settings=web-settings.json&budget=${this.budget}&webgl&noui&nofx&noreveal&run=${this.runId}`; }
  private async measure(): Promise<void> {
    try { await this.controller.runJavaScript('window.nextnewsWeb.measure().catch(e=>console.error(String(e)));'); }
    catch (error) { this.message = String(error); }
  }
  build() {
    Stack({ alignContent: Alignment.TopStart }) {
      Web({ src: this.url(), controller: this.controller })
        .width('100%').height('100%').javaScriptAccess(true).domStorageAccess(true)
        .onConsole((event) => {
          const text = event?.message.getMessage() ?? '';
          if (text.startsWith('WebTrial') || text.startsWith('WebBenchmark')) { console.info(text); this.message = text; }
          return false;
        })
      Column({ space: 8 }) {
        Row({ space: 8 }) {
          Button('返回').onClick(() => this.getUIContext().getRouter().back());
          Button(`${this.budget * 100} 万 Web`).onClick(() => { this.budget = this.budget === 2 ? 4 : this.budget === 4 ? 8 : 2; this.controller.loadUrl(this.url()); });
          Button(this.reference).onClick(() => { this.reference = this.reference === 'MetaFlow' ? 'SuperSplat' : 'MetaFlow'; this.runId = Date.now(); this.message = `${this.reference} · 等待首帧`; this.controller.loadUrl(this.url()); });
          Button('Web 测量').onClick(() => { this.measure(); });
        }
        Text(this.message).fontSize(11).fontColor('#EDF6FC').maxLines(3);
      }.padding(8).width('100%').backgroundColor('#BB07131F');
    }.width('100%').height('100%');
  }
}
