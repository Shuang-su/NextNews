import { Scene } from '@kit.ArkGraphics3D';
import { BusinessError } from '@kit.BasicServicesKit';
@Entry
@Component
struct Index {
  @State message: string = '等待基础 Scene.load';
  aboutToAppear(): void {
    Scene.load().then((scene: Scene) => {
      this.message = '基础场景创建成功';
      console.info('NextNewsBaseScene PASS');
      scene.destroy();
    }).catch((error: BusinessError) => {
      this.message = `基础场景失败：${error?.message ?? String(error)}`;
      console.error(`NextNewsBaseScene FAIL ${error?.message ?? String(error)}`);
    });
  }
  build() {
    Column() {
      Text('ArkGraphics3D 最小诊断').fontSize(24);
      Text('未导入 SpatialReconKit；未加载模型；未创建 C++ EGL 上下文').fontSize(16);
      Text(this.message).fontSize(20);
    }.padding(24).width('100%').height('100%');
  }
}
