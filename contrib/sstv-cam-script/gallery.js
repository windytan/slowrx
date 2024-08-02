function main() {
	var cards = document.getElementsByClassName("imgcard");
	for (var i = 0; i < cards.length; i++) {
		var card = cards[i];
		var expandbtn = document.createElement("button");
		expandbtn.textContent = "zoom";
		expandbtn.addEventListener("click", expand.bind(null, card));
		card.appendChild(expandbtn);

		var img = card.getElementsByTagName("img")[0];
		if (window.location.hash == ("#" + basename(img.src))) {
			expand(card);
		}
	}
}

function expand(card) {
	var img = card.getElementsByTagName("img")[0];
	window.location.hash = "#" + basename(img.src);

	var overlaydiv = document.createElement("div");
	var overlayimg = img.cloneNode(false);

	var overlayclose = document.createElement("button");
	overlayclose.href="#";
	overlayclose.addEventListener("click", close.bind(null, overlaydiv));
	overlayclose.appendChild(document.createTextNode("close"));

	var imgwidth = overlayimg.width;
	var imgheight = overlayimg.height;
	var viewportwidth = document.documentElement.clientWidth;
	var viewportheight = document.documentElement.clientHeight;

	/* Try fit width */
	var h = Math.round((viewportwidth * imgheight) / imgwidth);
	var w = Math.round((h * imgwidth) / imgheight);

	if ((w > viewportwidth) || (h > viewportheight)) {
		/* Fit height instead */
		w = Math.round((viewportheight * imgwidth) / imgheight);
		h = Math.round((w * imgheight) / imgwidth);
	}

	overlayimg.width = w;
	overlayimg.height = h;

	overlaydiv.classList.add("overlay");
	overlaydiv.addEventListener("click", close.bind(null, overlaydiv));
	overlaydiv.appendChild(overlayclose);
	overlaydiv.appendChild(overlayimg);
	card.parentElement.insertBefore(overlaydiv, card);
}

function close(overlaydiv) {
	overlaydiv.parentElement.removeChild(overlaydiv);
	window.location.hash = "";
}

function basename(uri) {
	var lastslash = uri.lastIndexOf("/");
	if (lastslash < 0) {
		return uri;
	} else {
		return uri.substring(lastslash + 1);
	}
}

window.addEventListener("DOMContentLoaded", main);
