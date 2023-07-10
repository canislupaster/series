import React, {useEffect, useMemo, useRef, useState} from 'react';
import {Box, Divider, Stack, StackProps, SxProps} from "@mui/material";

function DragStack({draggable, elements, moveElement, vertical, ...props}:
	StackProps & {draggable: boolean, elements: {key: string, el: React.ReactElement}[],
	moveElement: (from: number, to: number) => void, vertical: boolean}) {

	let [dragging, setDragging] = useState(-1);

	let els = elements.map((el,i) => {
		return (<div key={el.key} draggable={draggable} onDragStart={(e) => setDragging(i)} onDragEnd={(e) => {
			setDragging(-1);
			setDrop(-1);

			if (todrop!=-1) {
				moveElement(i, todrop<=i ? todrop : todrop-1);
			}
		}} >{el.el}</div>)
	});

	let [todrop, setDrop] = useState(-1);

	let stack_ref = useRef<HTMLDivElement|undefined>();
	let stack_sx: SxProps = {borderRadius: "5px"};

	if (vertical) stack_sx.width="100%", stack_sx.height="5px";
	else stack_sx.height="100%", stack_sx.width="5px";

	if (dragging!=-1) {
		for (let i=0; i<=elements.length; i++) {
			els.splice(2*i,0, <Box key={`drag${i}`} className={"dropper"} data-i={i} sx={{...stack_sx, backgroundColor: i==todrop ? "white" : "transparent"}} ></Box>);
		}
	}

	useEffect(() => {
		if (dragging==-1 || !stack_ref.current) return;

		let divs_to_i: [number,number][] = [];
		for (const child of stack_ref.current.children) {
			if (!child.classList.contains("dropper")) continue;
			divs_to_i.push([vertical ? child.getBoundingClientRect().top : child.getBoundingClientRect().left, parseInt((child as HTMLElement).dataset.i!)]);
		}

		let listen = (e) => {
			let v = vertical ? e.clientY : e.clientX;
			if (v==0) return; //i have no idea why

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

		let stack = stack_ref.current;
		return () => {
			stack.removeEventListener("dragover", listen);
			stack.removeEventListener("drop", drop_listen);
			stack.removeEventListener("dragleave", drag_leave);
		};
	}, [dragging]);

	return (<Stack {...(props as any)} ref={stack_ref} >
		{els}
	</Stack>);
}

export {DragStack};