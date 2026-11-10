#version 450 core

out vec4 FragColor;


in vec2 TexCoord;
flat in uint   shouldUseTexture;
flat in vec4  vPickingColor;
flat in uint usePixelPerfectPicking;


flat in uint   isPickable;

uniform sampler2D uTexture;
uniform float alphaThreshold = 0.1;

void main() {

       if(isPickable ==0u){
      discard;       }


    if (shouldUseTexture == 1u) {
        vec4 texColor = texture(uTexture, TexCoord);


if(usePixelPerfectPicking ==1u){
  if(texColor.a<alphaThreshold){
    discard;
  }
}
       
       FragColor =  vPickingColor;
    } else {
        FragColor =  vPickingColor;
    }
}
