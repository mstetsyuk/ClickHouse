-- { echo }
select 1 intersect select 1;
select 2 intersect select 1;
select 1 except select 1;
select 2 except select 1;

select number from numbers(5, 5) intersect select number from numbers(20);
select number from numbers(10) except select number from numbers(5);
select number, number+10 from numbers(12) except select number+5, number+15 from numbers(10);

select 1 except select 2 intersect select 1;
select 1 except select 2 intersect select 2;
select 1 intersect select 1 except select 2;
select 1 intersect select 1 except select 1;
select 1 intersect select 1 except select 2 intersect select 1 except select 3 intersect select 1;
select 1 intersect select 1 except select 2 intersect select 1 except select 3 intersect select 2;

select number from numbers(10) except select 5;
select number from numbers(100) intersect select number from numbers(20, 60) except select number from numbers(30, 20) except select number from numbers(60, 20);

with (select number from numbers(10) intersect select 5) as a select a * 10;
select count() from (select number from numbers(10) except select 5);
select count() from (select number from numbers(1000000) intersect select number from numbers(200000, 600000));
select count() from (select number from numbers(100) intersect select number from numbers(20, 60) except select number from numbers(30, 20) except select number from numbers(60, 20));
select count() from (select number from numbers(1000000) intersect select number from numbers(200000, 600000) except select number from numbers(300000, 200000) except select number from numbers(600000, 200000));

select 1 union all select 1 intersect select 1;
select 1 union all select 1 intersect select 2;
