/* Directly hosted on the card; zero external dependencies. */
U.page=location.hash.slice(1)||'overview';
if(!U.pages.some(function(p){return p[0]===U.page}))U.page='overview';
window.addEventListener('hashchange',function(){var p=location.hash.slice(1);if(U.pages.some(function(x){return x[0]===p})&&p!==U.page){U.page=p;U.render(true)}});
U.render(true);
U.poll();
U.timer=setInterval(function(){if(document.visibilityState==='visible')U.poll()},15000);
document.addEventListener('visibilitychange',function(){if(document.visibilityState==='visible'&&Date.now()-U.updated>15000)U.poll()});
