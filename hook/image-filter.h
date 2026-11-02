// Alpha-aware area reduction for straight-alpha ARGB game textures.
// No allocations or platform-specific APIs; the caller owns the output buffer.
#pragma once
static_assert(sizeof(unsigned int)==4);
inline bool reduceTexture(const unsigned int* source,int sw,int sh,
                          unsigned int* target,int tw,int th) {
    if(!source || !target || tw<1 || th<1 || sw<tw || sh<th) return false;
    const double dx=double(sw)/tw,dy=double(sh)/th,area=dx*dy;
    for(int y=0;y<th;++y) {
        double top=y*dy,bottom=(y+1)*dy;
        int yEnd=int(bottom);if(yEnd<bottom) ++yEnd;
        if(yEnd>sh) yEnd=sh;
        for(int x=0;x<tw;++x) {
            double left=x*dx,right=(x+1)*dx;
            int xEnd=int(right);if(xEnd<right) ++xEnd;
            if(xEnd>sw) xEnd=sw;
            double alpha=0,red=0,green=0,blue=0;
            for(int sy=int(top);sy<yEnd;++sy) {
                double a=top>sy?top:sy,b=bottom<sy+1?bottom:sy+1;
                for(int sx=int(left);sx<xEnd;++sx) {
                    double c=left>sx?left:sx,d=right<sx+1?right:sx+1;
                    double weight=(b-a)*(d-c);
                    unsigned int pixel=source[sy*sw+sx];
                    double coverage=(pixel>>24)*weight;
                    alpha+=coverage;
                    red+=((pixel>>16)&255)*coverage;
                    green+=((pixel>>8)&255)*coverage;
                    blue+=(pixel&255)*coverage;
                }
            }
            unsigned int aa=(unsigned int)(alpha/area+.5);
            if(!aa || alpha<=0) {target[y*tw+x]=0;continue;}
            unsigned int rr=(unsigned int)(red/alpha+.5),gg=(unsigned int)(green/alpha+.5),bb=(unsigned int)(blue/alpha+.5);
            if(aa>255) aa=255;
            if(rr>255) rr=255;
            if(gg>255) gg=255;
            if(bb>255) bb=255;
            target[y*tw+x]=(aa<<24)|(rr<<16)|(gg<<8)|bb;
        }
    }
    return true;
}
