// 视频播放处理
document.addEventListener('DOMContentLoaded', function() {
    // 视频封面点击处理
    const videoContainer = document.getElementById('videoContainer');
    if (videoContainer) {
        const videoCover = videoContainer.querySelector('.video-cover');
        const videoFrameContainer = videoContainer.querySelector('.video-frame-container');
        const autoplayIframe = document.getElementById('autoplayIframe');
        
        // 预加载封面图
        const preloadImg = new Image();
        preloadImg.src = 'assets/bilibili video cover.jpg';
        
        videoCover.addEventListener('click', function() {
            // 显示视频容器
            videoCover.style.opacity = '0';
            videoFrameContainer.style.display = 'block';
            
            // 设置真正的视频URL
            const realSrc = autoplayIframe.getAttribute('data-src');
            autoplayIframe.src = realSrc;
            
            // 淡入显示iframe
            setTimeout(function() {
                videoCover.style.display = 'none';
                autoplayIframe.style.opacity = '1';
            }, 50);
        });
    }
});
