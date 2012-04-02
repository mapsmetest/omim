#import "BalloonView.h"
#import <QuartzCore/CALayer.h>

@implementation BalloonView

@synthesize isDisplayed;

- (id) initWithTarget:(id)target andSelector:(SEL)selector;
{
  if ((self = [super init]))
  {
    isDisplayed = NO;
    m_target = target;
    m_selector = selector;
  }
  return self;
}

- (void) dealloc
{
  [m_buttonsView release];
  [m_pinView release];
  [super dealloc];
}

- (void) showButtonsInView:(UIView *)view atPoint:(CGPoint)pt
{
  m_buttonsView = [[UIImageView alloc] initWithImage:[UIImage imageNamed:@"star"]];
  CGFloat const w = m_buttonsView.bounds.size.width;
  CGFloat const h = m_buttonsView.bounds.size.height;
  m_buttonsView.frame = CGRectMake(pt.x - w/2, pt.y, w, 0);

  m_buttonsView.userInteractionEnabled = YES;
  UITapGestureRecognizer * recognizer = [[UITapGestureRecognizer alloc] initWithTarget:m_target action:m_selector];
  recognizer.numberOfTapsRequired = 1;
  recognizer.numberOfTouchesRequired = 1;
  recognizer.delaysTouchesBegan = YES;
  [m_buttonsView addGestureRecognizer:recognizer];
  [recognizer release];

  [view addSubview:m_buttonsView];
  // Animate buttons view to appear up from the pin
  [UIView transitionWithView:m_buttonsView duration:0.1 options:UIViewAnimationOptionCurveEaseIn
                  animations:^{
                    m_buttonsView.frame = CGRectMake(pt.x - w/2, pt.y - h, w, h);
                  } completion:nil];
}

- (void) showInView:(UIView *)view atPoint:(CGPoint)pt
{
  if (isDisplayed)
  {
    NSLog(@"Already displaying the BalloonView");
    return;
  }
  isDisplayed = YES;

  m_pinView = [[UIImageView alloc ]initWithImage:[UIImage imageNamed:@"marker"]];
  CGFloat const w = m_pinView.bounds.size.width;
  CGFloat const h = m_pinView.bounds.size.height;
  // Set initial frame at the top outside of the view
  m_pinView.frame = CGRectMake(pt.x - w/2, 0 - h, w, h);
  //pinView.userInteractionEnabled = YES;
  [view addSubview:m_pinView];
  // Animate pin to the touched point
  [UIView transitionWithView:m_pinView duration:0.1 options:UIViewAnimationOptionCurveEaseIn
                  animations:^{ m_pinView.frame = CGRectMake(pt.x - w/2, pt.y - h, w, h); }
                  completion:^(BOOL){
                    [self showButtonsInView:view atPoint:CGPointMake(pt.x, pt.y - h)];
                  }];
}

- (void) hide
{
  [m_buttonsView removeFromSuperview];
  [m_buttonsView release];
  m_buttonsView = nil;

  [m_pinView removeFromSuperview];
  [m_pinView release];
  m_pinView = nil;

  isDisplayed = NO;
}

@end
