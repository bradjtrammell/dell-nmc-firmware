/* H910P offline entry. The management view starts after OEM credential check. */
U.page=location.hash.slice(1)||'overview';
if(!U.pages.some(function(p){return p[0]===U.page}))U.page='overview';
window.addEventListener('hashchange',function(){
 var p=location.hash.slice(1);
 if(U.pages.some(function(x){return x[0]===p})&&p!==U.page){
  U.page=p;
  if(U.auth.header){U.render(true);if(U.loadPageData)U.loadPageData()}
 }
});
U.login();
document.addEventListener('visibilitychange',function(){
 if(U.auth.header&&document.visibilityState==='visible'&&Date.now()-U.updated>15000)U.poll()
});
