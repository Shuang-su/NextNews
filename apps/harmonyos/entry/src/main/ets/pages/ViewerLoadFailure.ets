// Native errors belong to the scene request that produced them. A previous
// failure must not reappear while a new scene is downloading its resources.
export function currentLoadFailure(state: string, message: string, failedRequest: number, currentRequest: number): string {
  if (state !== 'error' || currentRequest <= 0 || failedRequest !== currentRequest) { return ''; }
  if (message.startsWith('High-order SH requires encoded SOG pages')) {
    return '此模型包含高阶方向颜色，浮点分块暂不支持。请在设置中选择 SOG 编码纹理，或切换兼容模式。';
  }
  return message;
}
