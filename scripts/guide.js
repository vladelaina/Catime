document.addEventListener('DOMContentLoaded', function() {
    const videoContainer = document.getElementById('videoContainer');
    if (videoContainer) {
        const videoCover = videoContainer.querySelector('.video-cover');
        const videoFrameContainer = videoContainer.querySelector('.video-frame-container');
        const autoplayIframe = document.getElementById('autoplayIframe');
        
        const preloadImg = new Image();
        preloadImg.src = 'assets/bilibili video cover2.webp';
        
        videoCover.addEventListener('click', function() {
            videoCover.style.opacity = '0';
            videoFrameContainer.style.display = 'block';
            
            const realSrc = autoplayIframe.getAttribute('data-src');
            autoplayIframe.src = realSrc;
            
            setTimeout(function() {
                videoCover.style.display = 'none';
                autoplayIframe.style.opacity = '1';
            }, 50);
        });
    }
});


