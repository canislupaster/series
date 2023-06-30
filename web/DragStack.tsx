import React, {useEffect, useMemo, useRef, useState} from 'react';
import {Box, Box, Divider, Stack, StackProps} from "@mui/material";

function DragStack({elements, moveElement, vertical, ...props}: StackProps & {elements: {key: any, el: React.Component}[], moveElement: (from: number, to: number) => void, vertical: boolean}) {
	let [dragging, setDragging] = useState(-1);

	let els = elements.map((el,i) => {
		return (<div key={el.key} draggable={true} onDragStart={(e) => setDragging(i)} onDragEnd={(e) => {
			setDragging(-1);
			setDrop(-1);

			if (todrop!=-1) {
				moveElement(i, todrop<=i ? todrop : todrop-1);
			}
		}} >{el.el}</div>)
	});

	let [todrop, setDrop] = useState(-1);

	let stack_ref = useRef<HTMLDivElement|undefined>();
	let stack_sx = {borderRadius: "5px"};

	if (vertical) stack_sx.height="100%", stack_sx.width="5px";
	else stack_sx.width="100%", stack_sx.height="5px";

	if (dragging!=-1) {
		for (let i=0; i<=elements.length; i++) {
			els.splice(2*i,0, <Box key={`drag${i}`} className={"dropper"} data-i={i} sx={{...stack_sx, backgroundColor: i==todrop ? "white" : "transparent"}} ></Box>);
		}
	}

	useEffect(() => {
		if (dragging==-1 || !stack_ref.current) return;

		let divs_to_i = [];
		for (const child of stack_ref.current.children) {
			if (!child.classList.contains("dropper")) continue;
			divs_to_i.push([vertical ? child.getBoundingClientRect().left : child.getBoundingClientRect().top, parseInt(child.dataset.i)]);
		}

		let listen = (e) => {
			let v = vertical ? e.clientX : e.clientY;
			if (v==0) return; //i have no idea why
			let target = e.target as HTMLElement;

			let closest_i, closest_dist = Infinity;
			for (const [t,i] of divs_to_i) {
				let d = Math.abs(v-t);
				if (d<closest_dist) closest_i=i, closest_dist=d;
			}

			setDrop(closest_i);
			e.preventDefault();
		};

		stack_ref.current.addEventListener("dragover", listen);

		let drop_listen = (e) => e.preventDefault();
		stack_ref.current.addEventListener("drop", drop_listen);

		let drag_leave = (e) => setDrop(-1);
		stack_ref.current.addEventListener("dragleave", drag_leave);

		return () => {
			stack_ref.current.removeEventListener("dragover", listen);
			stack_ref.current.removeEventListener("drop", drop_listen);
			stack_ref.current.removeEventListener("dragleave", drag_leave);
		};
	}, [dragging]);

	return (<Stack {...props} ref={stack_ref} >
		{els}
	</Stack>);
}

export {DragStack};